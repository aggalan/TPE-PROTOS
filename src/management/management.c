#include "management.h"
#include "../metrics/metrics.h"
#include "../admin/admin.h"
#include "../args/args.h"
#include "../logging/logger.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>

#define MAX_LINE 512
#define MAX_UDP_PACKET 1024
struct socks5args socks5args;


typedef int (*cmd_handler_t)(char *args, char **out, size_t *outlen);

static int handle_stats(char *args, char **out, size_t *outlen) {
    (void)args;
    char *stats = metrics_to_string();
    if (!stats) return -1;
    LOG_DEBUG("Estadísticas: %s", stats);
    size_t n = snprintf(NULL, 0, "%s\n", stats);
    *out = malloc(n + 1);
    if (!*out) { return -1; }
    snprintf(*out, n + 1, "%s\n", stats);
    LOG_DEBUG("Estadísticas enviadas: %s", stats);
    *outlen = n;
    return 0;
}

static int handle_adduser(char *args, char **out, size_t *outlen) {
    char *username = strtok(args, " \t");
    char *password = strtok(NULL, " \t");
    if (!username || !password) return -1;
    if (admin_add_user(username, password) != 0) return -1;
    LOG_DEBUG("Usuario '%s' agregado", username);
    size_t n = snprintf(NULL, 0, "Usuario '%s' creado\n", username);
    *out = malloc(n + 1);
    if (!*out) return -1;
    snprintf(*out, n + 1, "Usuario '%s' creado\n", username);
    LOG_DEBUG("Usuario '%s' agregado", username);
    *outlen = n;
    return 0;
}

static int handle_deluser(char *args, char **out, size_t *outlen) {
    char *username = strtok(args, " \t");
    if (!username) return -1;
    if (admin_del_user(username) != 0) return -1;
    LOG_DEBUG("Usuario '%s' eliminado", username);
    size_t n = snprintf(NULL, 0, "Usuario '%s' eliminado\n", username);
    *out = malloc(n + 1);
    if (!*out) return -1;
    snprintf(*out, n + 1, "Usuario '%s' eliminado\n", username);
    LOG_DEBUG("Usuario '%s' eliminado", username);
    *outlen = n;
    return 0;
}

static int handle_listusers(char *args, char **out, size_t *outlen) {
    (void)args;
    char *list = admin_list_users();
    if (!list) return -1;
    size_t n = snprintf(NULL, 0, "%s\n", list);
    LOG_DEBUG("Lista de usuarios: %s", list);
    *out = malloc(n + 1);
    if (!*out) { free(list); return -1; }
    snprintf(*out, n + 1, "%s\n", list);
    *outlen = n;
    free(list);
    return 0;
}

static int handle_setauth(char *args, char **out, size_t *outlen) {
    if(strcmp(args, "enabled") == 0) {
         socks5args.authentication_enabled = true;
         LOG_DEBUG("Autenticación habilitada");
        *out = strdup("Autenticación habilitada\n");
        *outlen = strlen(*out);
    }
    else if (strcmp(args, "disabled") == 0) {
         socks5args.authentication_enabled = false;
         LOG_DEBUG("Autenticación deshabilitada");
         *out = strdup("Autenticación deshabilitada\n");
         *outlen = strlen(*out);
    } else {
        LOG_ERROR("Comando setauth inválido: %s", args);
        const char *msg = "ERROR: comando setauth inválido\n";
        *out = strdup(msg);
        *outlen = strlen(msg);
    }
    return 0;
}



static struct {
    const char *name;
    cmd_handler_t handler;
} commands[] = {
        {"stats",     handle_stats},
        {"adduser",   handle_adduser},
        {"deluser",   handle_deluser},
        {"listusers", handle_listusers},
        {"setauth",     handle_setauth},
        {NULL,         NULL}
};

static int process_udp_command(char *command, char **out, size_t *outlen) {
    char *eol = strchr(command, '\n');
    if (eol) *eol = '\0';

    char *cmd_copy = strdup(command);
    if (!cmd_copy) return -1;

    char *cmd = strtok(cmd_copy, " \t");
    char *args = strtok(NULL, "");
    char *response = NULL;
    size_t response_len = 0;
    int rc = -1;

    if (cmd) {
        for (int i = 0; commands[i].name; i++) {
            if (strcmp(cmd, commands[i].name) == 0) {
                if (commands[i].handler) {
                    rc = commands[i].handler(args, &response, &response_len);
                }
                break;
            }
        }
    }

    if (rc == 0) {
        LOG_DEBUG("Comando '%s' procesado correctamente", cmd);
        *out = response;
        *outlen = response_len;
    } else {
        LOG_ERROR("Comando '%s' fallido o no reconocido", cmd ? cmd : "NULL");
        const char *msg = "ERROR: comando inválido o fallo interno\n";
        *out = strdup(msg);
        *outlen = strlen(msg);
    }

    free(cmd_copy);
    return rc;
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

    char *response = NULL;
    size_t response_len = 0;
    process_udp_command(buffer, &response, &response_len);

    if (response && response_len > 0) {
        ssize_t sent = sendto(key->fd, response, response_len, 0,
                              (struct sockaddr*)&client_addr, client_len);
        if (sent < 0) {
            LOG_ERROR("Error sending UDP response: %s", strerror(errno));
        } else if ((size_t)sent != response_len) {
            LOG_WARNING("UDP response truncated: sent %zd of %zu bytes", sent, response_len);
        } else {
            LOG_DEBUG("UDP response sent successfully (%zu bytes)", response_len);
        }
        free(response);
    }
}