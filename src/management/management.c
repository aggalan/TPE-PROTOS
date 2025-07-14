#include "management.h"
#include "../metrics/metrics.h"
#include "../admin/admin.h"
#include "../args/args.h"
#include "../logging/logger.h"
#include "../admin/admin_logs.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>

#define MAX_UDP_PACKET 1024
#define MGMT_VERSION    1

// Method codes
enum {
    MGMT_LOGIN     = 1,
    MGMT_STATS     = 2,
    MGMT_ADDUSER   = 3,
    MGMT_DELUSER   = 4,
    MGMT_LISTUSERS = 5,
    MGMT_SETAUTH   = 6,
    MGMT_DUMP      = 7,
    MGMT_SEARCHLOGS= 8,
    MGMT_CLEARLOGS = 9
};

// Status codes
enum {
    MGMT_REQ           = 0,
    MGMT_OK_SIMPLE     = 20,
    MGMT_OK_WITH_DATA  = 21,
    MGMT_ERR_SYNTAX    = 40,
    MGMT_ERR_AUTH      = 41,
    MGMT_ERR_NOTFOUND  = 42,
    MGMT_ERR_INTERNAL  = 50
};

struct mgmt_hdr {
    uint8_t version;   // Protocol version
    uint8_t method;    // Command code
    uint8_t status;    // Status code
    uint16_t length;   // Payload length (bytes)
    uint8_t reserved;  // Reserved for future flags
};

static bool mgmt_is_logged_in = false;

static int send_response(int sockfd, struct sockaddr_in *cli_addr, socklen_t addrlen,
                         uint8_t method, uint8_t status,
                         const void *payload, uint16_t payload_len) {
    uint8_t buf[6 + 1024];
    if (payload_len > 1024) return -1;

    struct mgmt_hdr hdr = {
            .version = MGMT_VERSION,
            .method = method,
            .status = status,
            .length = htons(payload_len),
            .reserved = 0
    };

    memcpy(buf, &hdr, sizeof(hdr));
    if (payload_len > 0) {
        memcpy(buf + sizeof(hdr), payload, payload_len);
    }

    return sendto(sockfd, buf, sizeof(hdr) + payload_len, 0,
                  (struct sockaddr*)cli_addr, addrlen);
}

static void send_simple_response(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                                 uint8_t method, uint8_t status) {
    send_response(sockfd, cli, addrlen, method, status, NULL, 0);
}

static void send_data_response(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                               uint8_t method, const char *data) {
    if (data) {
        send_response(sockfd, cli, addrlen, method, MGMT_OK_WITH_DATA, data, strlen(data));
    } else {
        send_simple_response(sockfd, cli, addrlen, method, MGMT_ERR_INTERNAL);
    }
}

static bool parse_user_pass(const char *args, char *user, char *pass, size_t buf_size) {
    if (!args) return false;

    char fmt[32];
    snprintf(fmt, sizeof(fmt), "%%%zus %%%zus", buf_size - 1, buf_size - 1);
    return sscanf(args, fmt, user, pass) == 2;
}

// Helper to check authentication for all commands except login
static bool check_auth_and_respond(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                                   uint8_t method) {
    if (!mgmt_is_logged_in) {
        send_simple_response(sockfd, cli, addrlen, method, MGMT_ERR_AUTH);
        return false;
    }
    return true;
}

// Command handlers
static void handle_login(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                         const char *args) {
    if (mgmt_is_logged_in) {
        send_simple_response(sockfd, cli, addrlen, MGMT_LOGIN, MGMT_ERR_AUTH);
        return;
    }

    char user[64], pass[64];
    if (!parse_user_pass(args, user, pass, sizeof(user))) {
        send_simple_response(sockfd, cli, addrlen, MGMT_LOGIN, MGMT_ERR_SYNTAX);
        return;
    }

    if (strcmp(user, "admin") == 0 && strcmp(pass, "secret123") == 0) {
        mgmt_is_logged_in = true;
        send_simple_response(sockfd, cli, addrlen, MGMT_LOGIN, MGMT_OK_SIMPLE);
    } else {
        send_simple_response(sockfd, cli, addrlen, MGMT_LOGIN, MGMT_ERR_AUTH);
    }
}

static void handle_stats(int sockfd, struct sockaddr_in *cli, socklen_t addrlen) {
    char *stats = metrics_to_string();
    send_data_response(sockfd, cli, addrlen, MGMT_STATS, stats);
    free(stats);
}

static void handle_adduser(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                           const char *args) {
    char user[64], pass[64];
    if (!parse_user_pass(args, user, pass, sizeof(user))) {
        send_simple_response(sockfd, cli, addrlen, MGMT_ADDUSER, MGMT_ERR_SYNTAX);
        return;
    }

    uint8_t status = (admin_add_user(user, pass) == 0) ? MGMT_OK_SIMPLE : MGMT_ERR_INTERNAL;
    send_simple_response(sockfd, cli, addrlen, MGMT_ADDUSER, status);
}

static void handle_deluser(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                           const char *args) {
    if (!args || strlen(args) == 0) {
        send_simple_response(sockfd, cli, addrlen, MGMT_DELUSER, MGMT_ERR_SYNTAX);
        return;
    }

    uint8_t status = (admin_del_user(args) == 0) ? MGMT_OK_SIMPLE : MGMT_ERR_NOTFOUND;
    send_simple_response(sockfd, cli, addrlen, MGMT_DELUSER, status);
}

static void handle_listusers(int sockfd, struct sockaddr_in *cli, socklen_t addrlen) {
    char *users = admin_list_users();
    send_data_response(sockfd, cli, addrlen, MGMT_LISTUSERS, users);
    free(users);
}

static void handle_setauth(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                           const char *args) {
    if (!args || strlen(args) == 0) {
        send_simple_response(sockfd, cli, addrlen, MGMT_SETAUTH, MGMT_ERR_SYNTAX);
        return;
    }

    LOG_INFO("Setting new auth method: %s", args);
    send_simple_response(sockfd, cli, addrlen, MGMT_SETAUTH, MGMT_OK_SIMPLE);
}

static void handle_dump(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                        const char *args) {
    if (args && strlen(args) > 0) {
        send_simple_response(sockfd, cli, addrlen, MGMT_DUMP, MGMT_ERR_SYNTAX);
        return;
    }

    int param = (args && strlen(args) > 0) ? atoi(args) : 0;
    char *dump = dump_access(param);
    send_data_response(sockfd, cli, addrlen, MGMT_DUMP, dump);
    free(dump);
}

static void handle_searchlogs(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                              const char *args) {
    if (!args || strlen(args) == 0) {
        send_simple_response(sockfd, cli, addrlen, MGMT_SEARCHLOGS, MGMT_ERR_SYNTAX);
        return;
    }

    char *results = search_access(args);
    send_data_response(sockfd, cli, addrlen, MGMT_SEARCHLOGS, results);
    free(results);
}

static void handle_clearlogs(int sockfd, struct sockaddr_in *cli, socklen_t addrlen) {
    uint8_t status = (clean_logs() == 0) ? MGMT_OK_SIMPLE : MGMT_ERR_INTERNAL;
    send_simple_response(sockfd, cli, addrlen, MGMT_CLEARLOGS, status);
}

typedef void (*cmd_handler_t)(int sockfd, struct sockaddr_in *cli, socklen_t addrlen, const char *args);

struct command_info {
    uint8_t method;
    cmd_handler_t handler;
    bool needs_auth;
    bool has_args;
};

static void cmd_wrapper_no_args(int sockfd, struct sockaddr_in *cli, socklen_t addrlen, const char *args) {
    (void)args;
}

static const struct command_info commands[] = {
        {MGMT_LOGIN,     handle_login,     false, true},
        {MGMT_STATS,     (cmd_handler_t)handle_stats,     true,  false},
        {MGMT_ADDUSER,   handle_adduser,   true,  true},
        {MGMT_DELUSER,   handle_deluser,   true,  true},
        {MGMT_LISTUSERS, (cmd_handler_t)handle_listusers, true,  false},
        {MGMT_SETAUTH,   handle_setauth,   true,  true},
        {MGMT_DUMP,      handle_dump,      true,  true},
        {MGMT_SEARCHLOGS, handle_searchlogs, true, true},
        {MGMT_CLEARLOGS, (cmd_handler_t)handle_clearlogs, true,  false},
};

static const size_t num_commands = sizeof(commands) / sizeof(commands[0]);

void process_udp_command(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                         uint8_t method, const char *args) {
    const struct command_info *cmd = NULL;
    for (size_t i = 0; i < num_commands; i++) {
        if (commands[i].method == method) {
            cmd = &commands[i];
            break;
        }
    }

    if (!cmd) {
        send_simple_response(sockfd, cli, addrlen, method, MGMT_ERR_NOTFOUND);
        return;
    }

    if (cmd->needs_auth && !check_auth_and_respond(sockfd, cli, addrlen, method)) {
        return;
    }

    cmd->handler(sockfd, cli, addrlen, args);
}

void mgmt_udp_handle(struct selector_key *key) {
    LOG_DEBUG("Received UDP management packet on fd %d", key->fd);

    char buffer[MAX_UDP_PACKET];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    ssize_t n = recvfrom(key->fd, buffer, sizeof(buffer) - 1, 0,
                         (struct sockaddr*)&client_addr, &client_len);

    if (n <= 0) {
        LOG_ERROR("Error receiving UDP packet: %s", strerror(errno));
        return;
    }

    buffer[n] = '\0';
    LOG_DEBUG("Received UDP command: %s", buffer);

    process_udp_command(key->fd, &client_addr, client_len, buffer[0], buffer + 1);
}