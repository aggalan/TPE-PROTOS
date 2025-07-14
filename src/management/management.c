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

static bool mgmt_is_logged_in = false;
static struct sockaddr_in mgmt_logged_client = {0};
static time_t mgmt_last_seen = 0;

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

static bool is_same_client(struct sockaddr_in *a, struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr &&
           a->sin_port == b->sin_port;
}

static bool check_auth_and_respond(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                                   uint8_t method) {

    if (mgmt_is_logged_in && time(NULL) - mgmt_last_seen > MGMT_TIMEOUT) {
        mgmt_is_logged_in = false;
        memset(&mgmt_logged_client, 0, sizeof(mgmt_logged_client));
    }

    if (!mgmt_is_logged_in) {
        send_simple_response(sockfd, cli, addrlen, method, MGMT_ERR_AUTH);
        return false;
    }
    if (!is_same_client(cli, &mgmt_logged_client)) {
        send_simple_response(sockfd, cli, addrlen, method, MGMT_ERR_AUTH);
        return false;
    }

    return true;
}

static void handle_login(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                         const char *args) {

    if (mgmt_is_logged_in) {

        if (time(NULL) - mgmt_last_seen > MGMT_TIMEOUT) {
            mgmt_is_logged_in = false;
            memset(&mgmt_logged_client, 0, sizeof(mgmt_logged_client));
        }
        else if (!is_same_client(cli, &mgmt_logged_client)) {
            send_simple_response(sockfd, cli, addrlen, MGMT_LOGIN, MGMT_ERR_AUTH);
            return;
            } else {
                send_simple_response(sockfd, cli, addrlen, MGMT_LOGIN, MGMT_ERR_AUTH);
                return;
            }
    }

    char user[64], pass[64];
    if (!parse_user_pass(args, user, pass, sizeof(user))) {
        send_simple_response(sockfd, cli, addrlen, MGMT_LOGIN, MGMT_ERR_SYNTAX);
        return;
    }

    if (strcmp(user, "admin") == 0 && strcmp(pass, "secret123") == 0) {
        mgmt_is_logged_in = true;
        memcpy(&mgmt_logged_client, cli, sizeof(struct sockaddr_in));
        mgmt_last_seen = time(NULL);
        send_simple_response(sockfd, cli, addrlen, MGMT_LOGIN, MGMT_OK_SIMPLE);
    } else {
        send_simple_response(sockfd, cli, addrlen, MGMT_LOGIN, MGMT_ERR_AUTH);
    }
}

static void handle_logout(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                          const char *args) {
    (void)args;
    if (!mgmt_is_logged_in || !is_same_client(cli, &mgmt_logged_client)) {
        send_simple_response(sockfd, cli, addrlen, MGMT_LOGOUT, MGMT_ERR_AUTH);
        return;
    }

    mgmt_is_logged_in = false;
    memset(&mgmt_logged_client, 0, sizeof(mgmt_logged_client));
    send_simple_response(sockfd, cli, addrlen, MGMT_LOGOUT, MGMT_OK_SIMPLE);
}

static void handle_stats(int sockfd, struct sockaddr_in *cli, socklen_t addrlen, const char *args) {
    (void)args;
    char *stats = metrics_to_string();
    send_data_response(sockfd, cli, addrlen, MGMT_STATS, stats);
    ///free(stats);
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

static void handle_listusers(int sockfd, struct sockaddr_in *cli, socklen_t addrlen, const char *args) {
    (void)args;
    char *users = admin_list_users();
    send_data_response(sockfd, cli, addrlen, MGMT_LISTUSERS, users);
    //free(users);
}

static void handle_setauth(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                           const char *args) {
    if (!args || strlen(args) == 0) {
        send_simple_response(sockfd, cli, addrlen, MGMT_SETAUTH, MGMT_ERR_SYNTAX);
        return;
    }

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
    //free(dump);
}

static void handle_searchlogs(int sockfd, struct sockaddr_in *cli, socklen_t addrlen,
                              const char *args) {
    if (!args || strlen(args) == 0) {
        send_simple_response(sockfd, cli, addrlen, MGMT_SEARCHLOGS, MGMT_ERR_SYNTAX);
        return;
    }

    char *results = search_access(args);
    send_data_response(sockfd, cli, addrlen, MGMT_SEARCHLOGS, results);
    //free(results);
}

static void handle_clearlogs(int sockfd, struct sockaddr_in *cli, socklen_t addrlen, const char *args) {
    (void)args;
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


static const struct command_info commands[] = {
        {MGMT_LOGIN,     handle_login,     false, true},
        {MGMT_LOGOUT,     handle_logout,      true,  false},
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

    if (mgmt_is_logged_in) {
        mgmt_last_seen = time(NULL);
    }
}

void mgmt_udp_handle(struct selector_key *key) {

    char buffer[MAX_UDP_PACKET];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    ssize_t n = recvfrom(key->fd, buffer, sizeof(buffer) - 1, 0,
                         (struct sockaddr*)&client_addr, &client_len);

    if (n <= 0) {
        return;
    }

    if (n < 6) {
        return;
    }

    struct mgmt_hdr hdr;
    hdr.version  = buffer[0];
    hdr.method   = buffer[1];
    hdr.status   = buffer[2];
    hdr.length   = ((uint16_t)buffer[3] << 8) | buffer[4];  // network to host order (big endian)
    hdr.reserved = buffer[5];

    if (hdr.version != MGMT_VERSION) {
        send_simple_response(key->fd, &client_addr, client_len, hdr.method, MGMT_ERR_SYNTAX);
        return;
    }

    uint16_t payload_len = hdr.length;

    if (n < 6 + payload_len) {
        return;
    }

    char payload[MAX_UDP_PACKET] = {0};
    if (payload_len > 0) {
        memcpy(payload, buffer + 6, payload_len);
        payload[payload_len] = '\0';  // para usar como string
    }

    process_udp_command(key->fd, &client_addr, client_len, hdr.method, payload);
}

