
#define DEFAULT_ADDR "127.0.0.1"
#define DEFAULT_PORT 8080
#define BUFF_SIZE 1024

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <errno.h>
#include "../management/management.h"
#define MAX_PAYLOAD_LEN 1024
#define HEADER_SIZE 6
#define MAX_PACKET_LEN (HEADER_SIZE + MAX_PAYLOAD_LEN)

struct method_map {
    const char *name;
    uint8_t code;
};

static const struct method_map methods[] = {
    {"login", 1},
    {"stats", 2},
    {"adduser", 3},
    {"deluser", 4},
    {"listusers", 5},
    {"setauth", 6},
    {"dump", 7},
    {"searchlogs", 8},
    {"clearlogs", 9},
    {"exit", 10},
    {"addadmin", 11},
    {"deladmin", 12},
    {"listadmins", 13},
    {"setbuffer", 14},
    {NULL, 0}
};

int build_mgmt_packet(const char *input, unsigned char *out_packet, size_t *out_len);

struct admin_client {
    char buffer[BUFF_SIZE];
    int fd;
};

struct read {
    char buffer[BUFF_SIZE];
    int amount;
};

struct mgmt_datagram {
    uint8_t version;
    uint8_t method;
    uint8_t status;
    uint16_t length;
    uint8_t reserved;
    char payload[1024];
};

