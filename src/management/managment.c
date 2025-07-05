// management.c
// Módulo de administración integrado: create/delete users, stats, config

#include "../selector/selector.h"
#include "../logging/logger.h"
#include "../metrics/metrics.h"
#include "../admin/admin.h"
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
};


static void mgmt_close(struct selector_key *key) {
    struct mgmt_client *c = key->data;
    if (c) free(c);
//    close(key->fd);
}

static void mgmt_read(struct selector_key *key) {
    LOG_INFO("Management read event on fd %d", key->fd);
    struct mgmt_client *c = key->data;
    ssize_t n = read(c->fd, c->buf + c->len, MAX_LINE - c->len - 1);
    if (n <= 0) {
        selector_unregister_fd(key->s, key->fd);
        mgmt_close(key);
        return;
    }
    c->len += n;
    c->buf[c->len] = '\0';
    char *eol;
    while ((eol = strchr(c->buf, '\n')) != NULL) {
        *eol = '\0';
        char *cmd = strtok(c->buf, " \t");
        char resp[MAX_LINE];
        if (!cmd) {
            printf("ERROR: comando vacío\n");
        } else if (strcmp(cmd, "stats") == 0) {
            log_metrics();
        } else if (strcmp(cmd, "adduser") == 0) {
            char *u = strtok(NULL, " \t");
            char *p = strtok(NULL, " \t");
            if (u && p && admin_add_user(u, p) == 0)
                printf("OK: usuario '%s' creado\n", u);
            else
                printf("ERROR: no se pudo crear usuario\n");
        } else if (strcmp(cmd, "deluser") == 0) {
            char *u = strtok(NULL, " \t");
            if (u && admin_del_user(u) == 0)
                printf("OK: usuario '%s' eliminado\n", u);
            else
                printf("ERROR: no se pudo eliminar usuario\n");
        } else if (strcmp(cmd, "listusers") == 0) {
            char *list = admin_list_users();
            printf("%s\n", list);
            free(list);
        } else {
            printf("ERROR: comando '%s' desconocido\n", cmd);
        }
        write(c->fd, resp, strlen(resp));
        size_t used = (eol - c->buf) + 1;
        memmove(c->buf, eol + 1, c->len - used);
        c->len -= used;
        c->buf[c->len] = '\0';
        selector_unregister_fd(key->s, key->fd);
        mgmt_close(key);
        return;
    }
}

static void mgmt_accept(struct selector_key *key) {
    LOG_INFO("Management accept event on fd %d", key->fd);
    int lsock = key->fd;
    struct sockaddr_in addr;
    socklen_t alen = sizeof(addr);
    int fd = accept(lsock, (struct sockaddr*)&addr, &alen);
    if (fd < 0) return;
    fcntl(fd, F_SETFL, O_NONBLOCK);
    struct mgmt_client *c = calloc(1, sizeof(*c));
    c->fd = fd;
    c->len = 0;
    selector_register(key->s, fd, &(struct fd_handler){
            .handle_read  = mgmt_read,
            .handle_close = mgmt_close
    }, OP_READ, c);
}

const struct fd_handler management_handler = {
        .handle_read  = mgmt_accept,
        .handle_write = NULL,
        .handle_close = NULL
};
