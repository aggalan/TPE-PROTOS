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


#define MGMT_USER "admin"
#define MGMT_PASS "secret123"

#define MAX_LINE 512
#define MAX_UDP_PACKET 1024

typedef int (*cmd_handler_t)(char *args, char **out, size_t *outlen);

static bool mgmt_is_logged_in = false;


static int handle_login(char *args, char **out, size_t *outlen) {
    LOG_DEBUG("Login attempt - args: '%s'", args ? args : "NULL");

    if (!args) {
        LOG_ERROR("Login: No arguments provided");
        const char *err = "ERROR: uso: login <username> <password>\n";
        *out = strdup(err);
        *outlen = strlen(err);
        return -1;
    }

    char *args_copy = strdup(args);
    if (!args_copy) {
        LOG_ERROR("Login: Memory allocation failed");
        const char *err = "ERROR: error interno\n";
        *out = strdup(err);
        *outlen = strlen(err);
        return -1;
    }

    char *username = strtok(args_copy, " \t\n\r");
    char *password = strtok(NULL, " \t\n\r");

    LOG_DEBUG("Login: username='%s', password='%s'",
              username ? username : "NULL",
              password ? password : "NULL");

    if (!username || !password) {
        LOG_ERROR("Login: Missing username or password");
        const char *err = "ERROR: debe proporcionar usuario y contraseña\n";
        *out = strdup(err);
        *outlen = strlen(err);
        free(args_copy);
        return -1;
    }

    LOG_DEBUG("Login: Comparing with MGMT_USER='%s', MGMT_PASS='%s'", MGMT_USER, MGMT_PASS);

    if (strcmp(username, MGMT_USER) == 0 && strcmp(password, MGMT_PASS) == 0) {
        mgmt_is_logged_in = true;
        const char *msg = "OK: sesión iniciada\n";
        *out = strdup(msg);
        if (!*out) {
            LOG_ERROR("Login: Memory allocation failed for success message");
            free(args_copy);
            return -1;
        }
        *outlen = strlen(msg);
        LOG_INFO("Administrador autenticado correctamente");
        free(args_copy);
        return 0;
    }

    const char *err = "ERROR: credenciales inválidas\n";
    *out = strdup(err);
    if (!*out) {
        LOG_ERROR("Login: Memory allocation failed for error message");
        free(args_copy);
        return -1;
    }
    *outlen = strlen(err);
    LOG_WARNING("Intento de login fallido (user='%s')", username);
    free(args_copy);
    return -1;
}


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
        {"login",    handle_login},
        {"dump",    NULL},
        {"searchlogs", NULL},
        {"clearlogs", NULL},
        {NULL,         NULL}
};


static int process_udp_command(char *command, char **out, size_t *outlen) {
    LOG_DEBUG("Processing command: '%s'", command);

    char *eol = strchr(command, '\n');
    if (eol) *eol = '\0';

    eol = strchr(command, '\r');
    if (eol) *eol = '\0';

    char *cmd_copy = strdup(command);
    if (!cmd_copy) {
        LOG_ERROR("Memory allocation failed for command copy");
        const char *msg = "ERROR: error interno\n";
        *out = strdup(msg);
        *outlen = strlen(msg);
        return -1;
    }

    char *cmd = strtok(cmd_copy, " \t");
    char *args = strtok(NULL, "");

    LOG_DEBUG("Parsed command: cmd='%s', args='%s', logged_in=%d",
              cmd ? cmd : "NULL",
              args ? args : "NULL",
              mgmt_is_logged_in);

    if (!mgmt_is_logged_in && (!cmd || strcmp(cmd, "login") != 0)) {
        const char *msg = "ERROR: inicie sesión con 'login <user> <pass>'\n";
        *out = strdup(msg);
        *outlen = strlen(msg);
        LOG_WARNING("Comando '%s' rechazado: no autenticado", cmd ? cmd : "NULL");
        free(cmd_copy);
        return -1;
    }

    char *response = NULL;
    size_t response_len = 0;
    int rc = -1;

    if (cmd) {
        for (int i = 0; commands[i].name; i++) {
            if (strcmp(cmd, commands[i].name) == 0) {
                if (commands[i].handler) {
                    LOG_DEBUG("Executing handler for command '%s'", cmd);
                    rc = commands[i].handler(args, &response, &response_len);
                } else {
                    LOG_ERROR("No handler for command '%s'", cmd);
                }
                break;
            }
        }
    }

    if (rc == 0 && response) {
        LOG_DEBUG("Comando '%s' procesado correctamente", cmd);
        *out = response;
        *outlen = response_len;
    } else {
        LOG_ERROR("Comando '%s' fallido o no reconocido (rc=%d)", cmd ? cmd : "NULL", rc);
        if (response) free(response);
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