#ifndef _SOCKS5_H_
#define _SOCKS5_H_

#include "selector.h"

enum socks_v5state {
    GREETING_READ,
    GREETING_WRITE,
    DONE,
    ERROR,
};


struct socks5 {
    int                        client_fd;
    int                        origin_fd;

    buffer *                    read_buffer;
    buffer *                    write_buffer;
    bool                       closed;

    union {
        struct hello_st        hello;
    } client;

    struct state_machine       stm;
    struct sockaddr_storage    client_addr;
    socklen_t                  client_addr_len;
};


void socksv5_passive_accept(struct selector_key* key);
void socksv5_handle_read(struct selector_key* key);
void socksv5_handle_write(struct selector_key* key);
void socksv5_handle_close(struct selector_key* key);
void socksv5_pool_destroy(void);


#endif