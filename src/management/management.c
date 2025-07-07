#include "management.h"
#include "../metrics/metrics.h"
#include "../admin/admin.h"
#include "../logging/logger.h"

#include <stdlib.h>
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
    size_t buf_used;
    char *response;
    size_t response_len;
    size_t response_sent;
};

typedef int (*cmd_handler_t)(char *args, char **out, size_t *outlen);

static int handle_stats(char *args, char **out, size_t *outlen) {
    (void)args;
    char *stats = metrics_to_string();
    if (!stats) return -1;
    size_t n = snprintf(NULL, 0, "%s\n", stats);
    *out = malloc(n + 1);
    if (!*out) { return -1; }
    snprintf(*out, n + 1, "%s\n", stats);
    *outlen = n;
    return 0;
}

static int handle_adduser(char *args, char **out, size_t *outlen) {
    char *username = strtok(args, " \t");
    char *password = strtok(NULL, " \t");
    if (!username || !password) return -1;
    if (admin_add_user(username, password) != 0) return -1;
    size_t n = snprintf(NULL, 0, "Usuario '%s' creado\n", username);
    *out = malloc(n + 1);
    if (!*out) return -1;
    snprintf(*out, n + 1, "Usuario '%s' creado\n", username);
    *outlen = n;
    return 0;
}

static int handle_deluser(char *args, char **out, size_t *outlen) {
    char *username = strtok(args, " \t");
    if (!username) return -1;
    if (admin_del_user(username) != 0) return -1;
    size_t n = snprintf(NULL, 0, "Usuario '%s' eliminado\n", username);
    *out = malloc(n + 1);
    if (!*out) return -1;
    snprintf(*out, n + 1, "Usuario '%s' eliminado\n", username);
    *outlen = n;
    return 0;
}

static int handle_listusers(char *args, char **out, size_t *outlen) {
    (void)args;
    char *list = admin_list_users();
    if (!list) return -1;
    size_t n = snprintf(NULL, 0, "%s\n", list);
    *out = malloc(n + 1);
    if (!*out) { free(list); return -1; }
    snprintf(*out, n + 1, "%s\n", list);
    *outlen = n;
    free(list);
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
        {NULL,         NULL}
};

static void process_command(struct mgmt_client *c) {
    char *line = c->buf;
    char *eol = strchr(line, '\n');
    if (eol) *eol = '\0';
    char *cmd = strtok(line, " \t");
    char *args = strtok(NULL, "");
    char *out = NULL;
    size_t outlen = 0;
    int rc = -1;
    if (cmd) {
        for (int i = 0; commands[i].name; i++) {
            if (strcmp(cmd, commands[i].name) == 0) {
                rc = commands[i].handler(args, &out, &outlen);
                break;
            }
        }
    }
    if (rc == 0) {
        size_t n = snprintf(NULL, 0, "OK: %s", out);
        free(c->response);
        c->response = malloc(n + 1);
        snprintf(c->response, n + 1, "OK: %s", out);
        c->response_len = n;
    } else {
        const char *msg = "ERROR: comando inválido o fallo interno\n";
        free(c->response);
        c->response = strdup(msg);
        c->response_len = strlen(msg);
    }
    free(out);
    c->response_sent = 0;
    c->buf_used = 0;
    c->buf[0] = '\0';
}

void mgmt_read(struct selector_key *key) {
    struct mgmt_client *c = key->data;
    ssize_t n = read(c->fd, c->buf + c->buf_used, MAX_LINE - c->buf_used - 1);
    if (n <= 0) {
        selector_unregister_fd(key->s, key->fd);
        return;
    }
    c->buf_used += n;
    c->buf[c->buf_used] = '\0';
    if (strchr(c->buf, '\n')) {
        LOG_INFO("Received command: %s", c->buf);
        process_command(c);
        selector_set_interest(key->s, c->fd, OP_WRITE);
    }
}

void mgmt_write(struct selector_key *key) {
    struct mgmt_client *c = key->data;
    ssize_t n = write(c->fd, c->response + c->response_sent,
                      c->response_len - c->response_sent);
    if (n <= 0) {
        selector_unregister_fd(key->s, key->fd);
        return;
    }
    c->response_sent += n;
    if (c->response_sent >= c->response_len) {
        selector_set_interest(key->s, key->fd, OP_READ);
    }
}

void mgmt_close(struct selector_key *key) {
    struct mgmt_client *c = key->data;
    if (!c) return;
    free(c->response);
    close(c->fd);
    free(c);
}

void mgmt_accept(struct selector_key *key) {
    int fd = accept(key->fd, NULL, NULL);
    if (fd < 0) return;
    fcntl(fd, F_SETFL, O_NONBLOCK);
    struct mgmt_client *c = calloc(1, sizeof(*c));
    c->fd = fd;
    c->buf_used = 0;
    c->response = NULL;
    static const struct fd_handler handler = {
            .handle_read  = mgmt_read,
            .handle_write = mgmt_write,
            .handle_close = mgmt_close,
    };
    selector_register(key->s, fd, &handler, OP_READ, c);
}
