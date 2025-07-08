#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "../logging/logger.h"

#include "../buffer/buffer.h"

#define DEFAULT_ADDR "127.0.0.1"
#define DEFAULT_PORT 8080

#define ADMIN_USER "admin"
#define ADMIN_PASS "secret"

#define MAXLINE 512

static int readline(char *buf, size_t size) {
    if (!fgets(buf, size, stdin)) return -1;
    size_t len = strlen(buf);
    if (len && buf[len-1] == '\n') buf[len-1] = '\0';
    return 0;
}

static char *get_password(char *buf, size_t size) {
    struct termios oldt, newt;
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) return NULL;
    newt = oldt;
    newt.c_lflag &= ~ECHO;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) != 0) return NULL;
    printf("Password: "); fflush(stdout);
    if (!fgets(buf, size, stdin)) return NULL;
    printf("\n");
    size_t len = strlen(buf);
    if (len && buf[len-1] == '\n') buf[len-1] = '\0';
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return buf;
}

int main(int argc, char *argv[]) {
    const char *addr = DEFAULT_ADDR;
    unsigned short port = DEFAULT_PORT;

    if (argc >= 2) addr = argv[1];
    if (argc >= 3) port = (unsigned short)atoi(argv[2]);

    char user[64], pass[64];

    printf("Login required\n");
    printf("Username: "); fflush(stdout);
    if (readline(user, sizeof(user)) < 0) return 1;
    if (!get_password(pass, sizeof(pass))) return 1;

    if (strcmp(user, ADMIN_USER) != 0 || strcmp(pass, ADMIN_PASS) != 0) {
        fprintf(stderr, "Invalid credentials\n");
        return 1;
    }
    printf("Logged in as '%s'\n", ADMIN_USER);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    if (inet_pton(AF_INET, addr, &serv.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return 1;
    }
    if (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    uint8_t send_buf_data[MAXLINE];
    uint8_t recv_buf_data[MAXLINE];
    buffer send_buf;
    buffer recv_buf;
    buffer_init(&send_buf, sizeof(send_buf_data), send_buf_data);
    buffer_init(&recv_buf, sizeof(recv_buf_data), recv_buf_data);

    char line[MAXLINE];
    printf("Type 'help' for available commands, 'exit' to quit.\n");

    while (1) {
        printf("> "); fflush(stdout);
        if (readline(line, sizeof(line)) < 0) break;
        if (strcmp(line, "exit") == 0) break;
        if (strcmp(line, "help") == 0) {
            printf("Available commands:\n");
            printf("  stats             - show proxy metrics\n");
            printf("  listusers         - list configured users\n");
            printf("  adduser <u> <p>   - add a user with password\n");
            printf("  deluser <u>       - delete a user\n");
            printf("  help              - this help\n");
            printf("  exit              - quit client\n");
            continue;
        }

        buffer_reset(&send_buf);
        size_t cmdlen = strlen(line);
        for (size_t i = 0; i < cmdlen; i++) {
            buffer_write(&send_buf, (uint8_t)line[i]);
        }
        buffer_write(&send_buf, '\n');

        while (buffer_can_read(&send_buf)) {
            size_t nbyte;
            uint8_t *wptr = buffer_read_ptr(&send_buf, &nbyte);
            ssize_t nw = write(sock, wptr, nbyte);
            if (nw <= 0) {
                perror("write");
                goto done;
            }
            buffer_read_adv(&send_buf, nw);
        }

        buffer_reset(&recv_buf);
        while (1) {
            size_t wcap;
            uint8_t *bufp = buffer_write_ptr(&recv_buf, &wcap);
            LOG_DEBUG("Waiting for response, available space: %zu bytes\n", wcap);
            ssize_t nr = read(sock, bufp, wcap);
            if (nr <= 0) {
                perror("read");
                goto done;
            }
            buffer_write_adv(&recv_buf, nr);
            for (uint8_t *p = bufp; p < recv_buf.write; ++p) {
                if (*p == '\n') goto got_line;
            }
        }

        got_line:
        while (buffer_can_read(&recv_buf)) {
            putchar(buffer_read(&recv_buf));
        }
    }

    done:
    close(sock);
    printf("Goodbye.\n");
    return 0;
}
