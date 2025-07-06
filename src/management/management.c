#include "../selector//selector.h"
#include "../metrics/metrics.h"
#include "../admin/admin.h"
#include "management.h"
#include "../logging/logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

#define MAX_LINE 512

struct mgmt_client {
    int fd;
    char buf[MAX_LINE];
    size_t len;
    char *response;
    size_t response_len;
    size_t response_sent;
};

void mgmt_close(struct selector_key *key) {
    struct mgmt_client *c = key->data;
    if (c) {
        if (c->response) {
            free(c->response);
        }
        free(c);
    }
}

static void process_command(struct mgmt_client *c) {
    char *cmd = strtok(c->buf, " \t");
    char resp[MAX_LINE * 4] = {0};

    if (!cmd) {
        snprintf(resp, sizeof(resp), "ERROR: comando vacío\n");
    } else if (strcmp(cmd, "stats") == 0) {
        char *stats = metrics_to_string();
        if (stats) {
            snprintf(resp, sizeof(resp), "OK: %s\n", stats);
            free(stats);
        } else {
            snprintf(resp, sizeof(resp), "ERROR: no se pudo obtener estadísticas\n");
        }
    } else if (strcmp(cmd, "adduser") == 0) {
        char *u = strtok(NULL, " \t");
        char *p = strtok(NULL, " \t");
        if (u && p && admin_add_user(u, p) == 0) {
            snprintf(resp, sizeof(resp), "OK: usuario '%s' creado\n", u);
        } else {
            snprintf(resp, sizeof(resp), "ERROR: no se pudo crear usuario\n");
        }
    } else if (strcmp(cmd, "deluser") == 0) {
        char *u = strtok(NULL, " \t");
        if (u && admin_del_user(u) == 0) {
            snprintf(resp, sizeof(resp), "OK: usuario '%s' eliminado\n", u);
        } else {
            snprintf(resp, sizeof(resp), "ERROR: no se pudo eliminar usuario\n");
        }
    } else if (strcmp(cmd, "listusers") == 0) {
        LOG_INFO("Listing users\n");
        char *list = admin_list_users();
        if (list) {
            LOG_INFO("Users listed successfully\n");
            snprintf(resp, sizeof(resp), "OK: %s\n", list);
            free(list);
        } else {
            LOG_ERROR("Failed to list users\n");
            snprintf(resp, sizeof(resp), "ERROR: no se pudo obtener lista de usuarios\n");
        }
    } else {
        LOG_ERROR("Unknown command: %s\n", cmd);
        snprintf(resp, sizeof(resp), "ERROR: comando '%s' desconocido\n", cmd);
    }

    LOG_INFO("Response: %s\n", resp);
    c->response = strdup(resp);
    c->response_len = strlen(resp);
    c->response_sent = 0;
}

void mgmt_write(struct selector_key *key) {
    LOG_INFO("Writing response to management connection\n");
    struct mgmt_client *c = key->data;
    if (!c->response || c->response_sent >= c->response_len) {
        LOG_ERROR("ERROR: no hay datos para enviar\n");
        return;
    }

    ssize_t n = write(c->fd, c->response + c->response_sent, c->response_len - c->response_sent);
    if (n < 0) {
        LOG_ERROR("ERROR: fallo al escribir en el socket: %s\n", strerror(errno));
        selector_unregister_fd(key->s, key->fd);
        return;
    }

    c->response_sent += n;
    if (c->response_sent >= c->response_len) {
        selector_unregister_fd(key->s, key->fd);
    }
}

void mgmt_read(struct selector_key *key) {
    LOG_INFO("Reading from management connection\n");
    struct mgmt_client *c = key->data;
    ssize_t n = read(c->fd, c->buf + c->len, MAX_LINE - c->len - 1);
    if (n <= 0) {
        if (n == 0) {
            LOG_INFO("Management client disconnected\n");
        } else {
            LOG_ERROR("Error reading from management client: %s\n", strerror(errno));
        }
        selector_unregister_fd(key->s, key->fd);
        return;
    }
    c->len += n;
    c->buf[c->len] = '\0';

    char *eol = strchr(c->buf, '\n');
    if (eol) {
        *eol = '\0';
        LOG_INFO("Received command: %s\n", c->buf);
        process_command(c);
        selector_set_interest(key->s, key->fd, OP_WRITE);
    }
}

void mgmt_accept(struct selector_key *key) {
    LOG_INFO("New management connection accepted\n");
    struct sockaddr_in addr;
    socklen_t alen = sizeof(addr);
    int fd = accept(key->fd, (struct sockaddr*)&addr, &alen);
    if (fd < 0){
        LOG_ERROR("Error accepting management connection: %s", strerror(errno));
        return;
    };

    fcntl(fd, F_SETFL, O_NONBLOCK);

    struct mgmt_client *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        LOG_ERROR("Failed to allocate memory for management client");
        close(fd);
        return;
    }
    c->fd = fd;
    c->len = 0;
    c->response = NULL;
    c->response_len = 0;
    c->response_sent = 0;

    selector_register(key->s, fd, &(struct fd_handler){
            .handle_read  = mgmt_read,
            .handle_write = mgmt_write,
            .handle_close = mgmt_close
    }, OP_READ, c);
}