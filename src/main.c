/**
 * main.c - servidor proxy socks concurrente + módulo de management
 *
 * Interpreta los argumentos de línea de comandos, y monta sockets
 * pasivos para proxy (SOCKS) y para administración (management).
 *
 * Ambas interfaces se sirven en el mismo proceso usando el selector.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include "./admin/admin_logs.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "args/args.h"
#include "socks5/socks5.h"
#include "selector/selector.h"
#include "logging/logger.h"
#include "metrics/metrics.h"
#include "management/management.h"

static bool done = false;
struct socks5args socks5args;


static void sigterm_handler(const int signal) {
    (void) signal;
    LOG_DEBUG("signal %d, cleaning up and exiting\n", signal);
    done = true;
}

static int setup_listener(const char* addr, unsigned short port) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    inet_pton(AF_INET, addr, &sa.sin_addr);
    sa.sin_port = htons(port);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) return -1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if (bind(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        close(sock);
        return -1;
    }
    if (listen(sock, 20) < 0) {
        close(sock);
        return -1;
    }
    if (selector_fd_set_nio(sock) == -1) {
        close(sock);
        return -1;
    }
    return sock;
}

static int setup_udp_listener(const char* addr, unsigned short port) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    inet_pton(AF_INET, addr, &sa.sin_addr);
    sa.sin_port = htons(port);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return -1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if (bind(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        close(sock);
        return -1;
    }
    if (selector_fd_set_nio(sock) == -1) {
        close(sock);
        return -1;
    }
    return sock;
}

int main(const int argc, const char** argv) {
    logger_init();
    metrics_init();
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    parse_args(argc, (char**)argv, &socks5args);
    LOG_INFO("Proxy SOCKS en %s:%hu, Management en %s:%hu\n",
             socks5args.socks_addr, socks5args.socks_port,
             socks5args.mng_addr,    socks5args.mng_port);
    socks5args.authentication_enabled = false;
    socks5args.buffer_size = BUFFER_SIZE; // Default buffer size
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    const struct selector_init conf = {
            .signal = SIGALRM,
            .select_timeout = {.tv_sec = 10, .tv_nsec = 0},
    };
    if (0 != selector_init(&conf)) {
        perror("initializing selector");
        return 1;
    }
    fd_selector selector = selector_new(1024);
    if (selector == NULL) {
        perror("creating selector");
        return 1;
    }

    int socks_sock = setup_listener(socks5args.socks_addr, socks5args.socks_port);
    if (socks_sock < 0) {
        perror("bind/listen SOCKS");
        return 1;
    }
    const struct fd_handler socksv5 = {
            .handle_read  = socksv5_passive_accept,
            .handle_write = NULL,
            .handle_close = NULL,
    };
    selector_register(selector, socks_sock, &socksv5, OP_READ, NULL);



    int mng_sock = setup_udp_listener(socks5args.mng_addr, socks5args.mng_port);
    if (mng_sock < 0) {
        perror("bind/listen management");
        return 1;
    }
    const struct fd_handler management_handler = {
            .handle_read  = mgmt_udp_handle,
            .handle_write = NULL,
            .handle_close = NULL
    };
    selector_register(selector, mng_sock, &management_handler, OP_READ, NULL);

    while (!done) {
        if (selector_select(selector) != SELECTOR_SUCCESS) {
            perror("serving");
            break;
        }
    }

    clean_logs();
    selector_destroy(selector);
    selector_close();
    close(socks_sock);
    close(mng_sock);
    return 0;
}
