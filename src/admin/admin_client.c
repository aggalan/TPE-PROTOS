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
    puts("  listusers [N pages] - list configured N users \n");
    puts("  listadmins [N pages] - list configured N admins\n");
    puts("  addadmin <u> <p>   - add a admin with password\n");
    puts("  adduser <u> <p>   - add a user with password\n");
    puts("  deluser <u>       - delete a user\n");
    puts("  deladmin <u>      - delete an admin\n");
    puts("  setbuffer <size> - set buffer size for the proxy\n");
    puts("  setauth <enabled|disabled> - enable or disable authentication\n");
    puts("  login <u> <p>     - authenticate user\n");
    puts("  dump [N lines]    - dump last N lines of access logs (default 10)\n");
    puts("  searchlogs <u>    - search logs for user <u>\n");
    puts("  clearlogs         - clear all log entries\n");
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
    printf("Don't forget to login with 'login <user> <pass>' before sending commands.\n");

    while (1) {
        printf("> "); fflush(stdout);
        if ((read_buff->amount = (int) read(STDIN_FILENO, read_buff->buffer, BUFF_SIZE)) < 0) {
            printf("Nothing to read");
            close(client->fd);
            break;
        }

        read_buff->buffer[strcspn(read_buff->buffer, "\n")] = 0;
        read_buff->amount = strlen(read_buff->buffer);

        if (strcmp(read_buff->buffer, "exit") == 0) {
            unsigned char logout_packet[MAX_PACKET_LEN];
            size_t logout_len;

            if (build_mgmt_packet(read_buff->buffer, logout_packet, &logout_len) == 0) {
                sendto(client->fd, logout_packet, logout_len, 0,
                       (struct sockaddr*)&serv, serv_len);

                struct sockaddr_in from_addr;
                socklen_t from_len = sizeof(from_addr);
                ssize_t resp_len = recvfrom(client->fd, client->buffer, sizeof(client->buffer) - 1, 0,
                                            (struct sockaddr*)&from_addr, &from_len);

                if (resp_len > 0) {
                    struct mgmt_hdr *hdr = (struct mgmt_hdr *)client->buffer;
                    if (hdr->method == MGMT_LOGOUT && hdr->status == MGMT_OK_SIMPLE) {
                        printf("[INFO] Logged out successfully.\n");
                    }
                }
            }

            break;
        }

        if (strcmp(read_buff->buffer, "help") == 0) {
            help();
            continue;
        }

        unsigned char packet[MAX_PACKET_LEN];
        size_t packet_len;

        if (build_mgmt_packet(read_buff->buffer, packet, &packet_len) != 0) {
            fprintf(stderr, "Failed to build packet\n");
            continue;
        }

        ssize_t n = sendto(client->fd, packet, packet_len, 0,
                           (struct sockaddr*)&serv, serv_len);
        if (n != (ssize_t)packet_len) {
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
        }
        struct mgmt_hdr *resp_hdr = (struct mgmt_hdr *)client->buffer;

        if (resp_hdr->version != MGMT_VERSION) {
            fprintf(stderr, "Received response with wrong version (%u)\n", resp_hdr->version);
            continue;
        }

        const char *status_msg = NULL;
        switch (resp_hdr->status) {
            case MGMT_OK_SIMPLE:
                status_msg = "[OK] Command executed successfully.";
                break;
            case MGMT_OK_WITH_DATA:
                status_msg = "[OK] Command returned data:";
                break;
            case MGMT_ERR_SYNTAX:
                status_msg = "[ERROR] Invalid syntax.";
                break;
            case MGMT_ERR_AUTH:
                status_msg = "[ERROR] Invalid credentials.";
                break;
            case MGMT_ERR_NO_AUTH:
                status_msg = "[ERROR] Unauthorized, you must log in first.";
                break;
            case MGMT_ERR_EXPIRED:
                status_msg = "[ERROR] Session expired. Please log in again.";
                break;
            case MGMT_ERR_LOGGED_IN:
                status_msg = "[ERROR] Already logged in.";
                break;
            case MGMT_ERR_NOTFOUND:
                status_msg = "[ERROR] User or resource not found.";
                break;
            case MGMT_ERR_INTERNAL:
                status_msg = "[ERROR] Internal server error.";
                break;
            case MGMT_ERR_NOT_SUPPORTED:
                status_msg = "[ERROR] Unsupported command or version.";
                break;
            case MGMT_ERR_BUSY:
                status_msg = "[ERROR] Another admin is logged, please wait.";
                break;
            default:
                status_msg = "[ERROR] Unknown status code.";
                break;
        }

        printf("%s\n", status_msg);

        if (resp_hdr->status == MGMT_OK_WITH_DATA) {
            char *payload = (char *)(client->buffer + sizeof(struct mgmt_hdr));
            payload[bytes_read - sizeof(struct mgmt_hdr)] = '\0';
            puts(payload);
        }
        memset(client->buffer, 0, bytes_read);
        memset(read_buff->buffer, 0, read_buff->amount);
    }

    close(client->fd);
    free(read_buff);
    free(client);
    return 0;
}

int build_mgmt_packet(const char *input, unsigned char *out_packet, size_t *out_len) {
    if (!input || !out_packet || !out_len) return -1;

    char cmd[32];
    const char *args = strchr(input, ' ');
    size_t cmd_len;

    if (args) {
        cmd_len = args - input;
        if (cmd_len >= sizeof(cmd)) return -1;
        strncpy(cmd, input, cmd_len);
        cmd[cmd_len] = '\0';
        args++;
    } else {
        strncpy(cmd, input, sizeof(cmd));
        cmd[sizeof(cmd)-1] = '\0';
        args = "";
    }

    uint8_t method = 0;
    for (int i = 0; methods[i].name != NULL; i++) {
        if (strcmp(cmd, methods[i].name) == 0) {
            method = methods[i].code;
            break;
        }
    }

    if (method == 0) {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        return -1;
    }

    size_t payload_len = strlen(args);
    if (payload_len > MAX_PAYLOAD_LEN) {
        fprintf(stderr, "Payload too long\n");
        return -1;
    }

    out_packet[0] = 1;
    out_packet[1] = method;
    out_packet[2] = 0;
    out_packet[3] = (payload_len >> 8) & 0xFF;
    out_packet[4] = payload_len & 0xFF;
    out_packet[5] = 0;

    memcpy(out_packet + HEADER_SIZE, args, payload_len);
    *out_len = HEADER_SIZE + payload_len;
    return 0;
}