#ifndef _SOCKS5_H_
#define _SOCKS5_H_

#include "selector.h"
#include "buffer.h"
#include "./negotiation/negotiation.h"
#include "stm.h"
#include <netdb.h>
#include <stdbool.h>
#include <sys/socket.h>

enum socks_v5state {
    NEGOTIATION_READ,
    NEGOTIATION_WRITE,
    DONE,
    ERROR,
};

#define ATTACHMENT(key) ((struct socks5 *)(key)->data)

typedef struct socks5 {
    int                        client_fd;
    int                        origin_fd;
    buffer                     read_buffer;
    buffer                     write_buffer;
    bool                       closed;
    struct state_machine       stm;
    struct sockaddr_storage    client_addr;
    socklen_t                  client_addr_len;
    union {
        NegParser negotiation_parser;
    } client;

}SocksClient;


void socksv5_passive_accept(struct selector_key* key);
void socksv5_handle_read(struct selector_key* key);
void socksv5_handle_write(struct selector_key* key);
void socksv5_handle_close(struct selector_key* key);
void socksv5_pool_destroy(void);


#endif