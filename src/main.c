// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "args.h"

#define BUFFER_SIZE 1024
#define MAX_CLIENTS  FD_SETSIZE

static int
make_listener(const char *addr, unsigned short port)
{
    int fd, on = 1;
    struct sockaddr_in sa = {
            .sin_family = AF_INET,
            .sin_port   = htons(port),
    };

    if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        perror("setsockopt");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (bind(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror("bind");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (listen(fd, SOMAXCONN) < 0) {
        perror("listen");
        close(fd);
        exit(EXIT_FAILURE);
    }

    return fd;
}

int
main(int argc, char *argv[])
{
    struct socks5args args;
    fd_set  master_fds, read_fds;
    int     fdmax, socks_fd, mng_fd;
    char    buffer[BUFFER_SIZE];

    parse_args(argc, argv, &args);

    socks_fd = make_listener(args.socks_addr, args.socks_port);
    printf("SOCKS listener on %s:%u\n",
           args.socks_addr, args.socks_port);

    mng_fd   = make_listener(args.mng_addr, args.mng_port);
    printf("Management listener on %s:%u  dissectors %s\n",
           args.mng_addr, args.mng_port,
           args.disectors_enabled ? "ON" : "OFF");

    FD_ZERO(&master_fds);
    FD_SET(socks_fd, &master_fds);
    FD_SET(mng_fd,   &master_fds);

    fdmax = (socks_fd > mng_fd ? socks_fd : mng_fd);

    while(1) {
        read_fds = master_fds;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        for (int fd = 0; fd <= fdmax; fd++) {
            if (!FD_ISSET(fd, &read_fds)) continue;

            if (fd == socks_fd || fd == mng_fd) {
                struct sockaddr_in peer;
                socklen_t len = sizeof(peer);
                int new_fd = accept(fd, (struct sockaddr*)&peer, &len);
                if (new_fd < 0) {
                    perror("accept");
                    continue;
                }
                FD_SET(new_fd, &master_fds);
                if (new_fd > fdmax) fdmax = new_fd;
                printf("New %s connection on socket %d from %s:%u\n",
                       fd == socks_fd ? "SOCKS" : "MGMT",
                       new_fd,
                       inet_ntoa(peer.sin_addr),
                       ntohs(peer.sin_port));
            } else {
//                DESDE ACA ES LO QUE SE DEBE CAMBIAR
                ssize_t n = recv(fd, buffer, sizeof(buffer)-1, 0);
                if (n <= 0) {
                    if (n == 0) {
                        printf("Socket %d closed by peer\n", fd);
                    } else {
                        perror("recv");
                    }
                    close(fd);
                    FD_CLR(fd, &master_fds);
                } else {
                    buffer[n] = '\0';
                    printf("Received on %d: %s\n", fd, buffer);
                    if (fd != mng_fd) {
                        if (write(fd, buffer, n) < 0)
                            perror("write");
                    }
                }
//                HASTA ACA ES LO QUE SE DEBE CAMBIAR

            }
        }
    }

    close(socks_fd);
    close(mng_fd);
    return 0;
}
