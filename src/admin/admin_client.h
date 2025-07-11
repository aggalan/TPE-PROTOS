
#define DEFAULT_ADDR "127.0.0.1"
#define DEFAULT_PORT 8080
#define BUFF_SIZE 1024

#define ADMIN_USER "admin"
#define ADMIN_PASS "secret"

#define MAXLINE 512
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>  // socket
#include <errno.h>

struct admin_client {
    char buffer[BUFF_SIZE];
    int fd;
};

struct read {
    char buffer[BUFF_SIZE];
    int amount;
};
