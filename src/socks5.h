#ifndef _SOCKS5_H_
#define _SOCKS5_H_

#include "selector.h"
#include "buffer.h"
#include "relay/relay.h"
#include "./negotiation/negotiation.h"
#include "./authentication/authentication.h"
#include "./request/request.h"
#include "stm.h"
#include "authentication/authentication_parser.h"
#include "request/request_parser.h"
#include <netdb.h>
#include <stdbool.h>
#include <sys/socket.h>

enum socks_v5state {
    NEGOTIATION_READ =0,
    NEGOTIATION_WRITE,
    AUTHENTICATION_READ ,
    AUTHENTICATION_WRITE,
    REQUEST_READ,
    REQUEST_WRITE,
    REQUEST_CONNECTING,
    RELAY,
    DONE,
    ERROR,
    ORIGIN_CONNECT,
    ORIGIN_CONNECT_WRITE,
};

struct relay {
    int *fd;

    buffer *rb, *wb;

    fd_interest duplex;

    struct relay *other;
};


#define ATTACHMENT(key) ((struct socks5 *)(key)->data)
#define BUFFER_SIZE 4096
typedef struct socks5 {
    int                        client_fd;
    int                        origin_fd;
    buffer                     read_buffer;
    buffer                     write_buffer;
    uint8_t read_buffer_space[BUFFER_SIZE];
    uint8_t write_buffer_space[BUFFER_SIZE];
    bool                       closed;
    struct state_machine       stm;
    struct sockaddr_storage    client_addr;
    struct addrinfo*           origin_resolution;
    socklen_t                  client_addr_len;
    union {
        struct relay relay;
        NegParser negotiation_parser;
        AuthParser authentication_parser;
        ReqParser request_parser;
    } client;
    union {
        struct relay relay;
    }origin;

}SocksClient;


void socksv5_passive_accept(struct selector_key* key);
void socksv5_handle_read(struct selector_key* key);
void socksv5_handle_write(struct selector_key* key);
void socksv5_handle_close(struct selector_key* key);
void socksv5_pool_destroy(void);
const fd_handler * get_fd_handler();


#endif