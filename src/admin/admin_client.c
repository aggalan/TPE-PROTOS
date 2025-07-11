#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "admin_client.h"

void help(){
    puts("Available commands:\n");
    puts("  stats             - show proxy metrics\n");
    puts("  listusers         - list configured users\n");
    puts("  adduser <u> <p>   - add a user with password\n");
    puts("  deluser <u>       - delete a user\n");
    puts("  help              - this help\n");
    puts("  exit              - quit client\n");
}

int main(int argc, char *argv[]) {
    const char *addr = DEFAULT_ADDR;
    unsigned short port = DEFAULT_PORT;

    if (argc >= 2) addr = argv[1];
    if (argc >= 3) port = (unsigned short)atoi(argv[2]);

    struct admin_client *client = calloc(1, sizeof(struct admin_client));
    struct read *read_buff = calloc(1, sizeof(struct read));

    if (client == NULL || read_buff == NULL) {
        perror("calloc");
        return 1;
    }

    char user[64], pass[64];

    puts("Login required");
    printf("Username: ");
    if (fgets(user, sizeof user, stdin) == NULL) {
        perror("fgets");
        return 1;
    }
    user[strcspn(user, "\n")] = 0;
    printf("Password: ");
    if (fgets(pass, sizeof pass, stdin) == NULL) {
        perror("fgets");
        return 1;
    }
    pass[strcspn(pass, "\n")] = 0;

    if (strcmp(user, ADMIN_USER) != 0 || strcmp(pass, ADMIN_PASS) != 0) {
        fputs("Authentication failed\n", stderr);
        return 1;
    }
    printf("Authenticated!  Connecting to %s:%hu …\n", addr, port);

    struct sockaddr_in serv;
    serv.sin_addr.s_addr = inet_addr(addr);
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    socklen_t serv_len = sizeof(serv);

    if ((client->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Error creating client socket");
        exit(1);
    }

    printf("UDP socket created. Ready to send commands.\n");
    printf("Type 'help' for available commands, 'exit' to quit.\n");

    while (1) {
        printf("> "); fflush(stdout);
        if ((read_buff->amount = (int) read(STDIN_FILENO, read_buff->buffer, BUFF_SIZE)) < 0) {
            printf("Nothing to read");
            close(client->fd);
            break;
        }

        read_buff->buffer[strcspn(read_buff->buffer, "\n")] = 0;
        read_buff->amount = strlen(read_buff->buffer);

        if (strcmp(read_buff->buffer, "exit") == 0) break;

        if (strcmp(read_buff->buffer, "help") == 0) {
            help();
            continue;
        }

        ssize_t n = sendto(client->fd, read_buff->buffer, read_buff->amount, 0,
                           (struct sockaddr*)&serv, serv_len);
        if (n != read_buff->amount) {
            fprintf(stderr, "sendto() sent unexpected number of bytes.\n");
        }

        errno = 0;
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        ssize_t bytes_read = recvfrom(client->fd, client->buffer, sizeof(client->buffer) - 1, 0,
                                      (struct sockaddr*)&from_addr, &from_len);
        if (bytes_read <= 0) {
            fprintf(stderr, "recvfrom() failed: %s\n", strerror(errno));
            exit(1);
        } else {
            client->buffer[bytes_read] = '\0';
            printf("%s", client->buffer);
            memset(client->buffer, 0, bytes_read);
            memset(read_buff->buffer, 0, read_buff->amount);
        }
    }

    close(client->fd);
    free(read_buff);
    free(client);
    return 0;
}