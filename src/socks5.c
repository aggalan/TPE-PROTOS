#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "selector.h"
#include "socks5.h"
#include "buffer.h"
#include "stm.h"
#include "parser.h"

#define BUFFER_SIZE 4096

#define ATTACHMENT(key) ((struct socks5 *)(key)->data)

#include "./greeting/greeting.h"





static void socksv5_read(struct selector_key *key);
static void socksv5_write(struct selector_key *key);
static void socksv5_close(struct selector_key *key);
static const struct fd_handler socks5_handler = {
    .handle_read = socksv5_read,
    .handle_write = socksv5_write,
    .handle_close = socksv5_close,
};



struct socks5 *socks5_new(const int client_fd,
                           const struct sockaddr_storage *client_addr,
                           const socklen_t client_addr_len))
{
    struct socks5 *ret = calloc(1, sizeof(*ret));
    if (ret == NULL)
    {
        goto fail;
    }

    ret->client_fd = client_fd;
    ret->origin_fd = -1;
    ret->client_addr = client_addr;
    ret->client_addr_len = client_addr_len;


    static uint8_t r_buffer[BUFFER_SIZE], w_buffer[BUFFER_SIZE];
    buffer_init(&ret->read_buffer, BUFFER_SIZE, r_buffer);
    buffer_init(&ret->write_buffer, BUFFER_SIZE, w_buffer);
    ret->stm.initial = GREETING_READ;
    ret->stm.max_state = ERROR;
    ret->closed = false;
    ret->hello = calloc(sizeof(struct hello_st));
    if (ret->hello == NULL)
    {
        goto fail;
    }

    stm_init(&ret->stm);

    return ret;

fail:
    if (ret != NULL)
    {
        free(ret);
    }
    return NULL;
}


void socksv5_passive_accept(struct selector_key *key)
{
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    struct socks5 *state = NULL;

    const int client = accept(key->fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client == -1)
    {
        goto fail;
    }
    if (selector_fd_set_nio(client) == -1)
    {
        goto fail;
    }
    state = socks5_new(client, &client_addr, client_addr_len);
    if (state == NULL)
    {
        goto fail;
    }
    if (SELECTOR_SUCCESS != selector_register(key->s, client, &socks5_handler,OP_READ, state))
    {
        goto fail;
    }
    return;

fail:
    if (client != -1)
    {
        close(client);
    }
    socks5_destroy(state);
}


static const struct state_definition client_statbl[] = {
        {
                .state        = NEGOTIATION_READ,
                .on_arrival   = greeting_init,
                .on_read_ready  = greeting_read,
        },
        {
                .state        = NEGOTIATION_WRITE,
                .on_write_ready = greeting_write,
        },
        {
                .state        = DONE,
                .on_departure = socksv5_done,
        },
        {
                .state        = ERROR,
                .on_departure = socksv5_done,
        }
};


static void socksv5_read(struct selector_key *key)
{
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum socks_v5state st = stm_handler_read(stm, key);

    if (ERROR == st || DONE == st)
    {
        socksv5_done(key);
    }
}

static void
socksv5_write(struct selector_key *key)
{
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum socks_v5state st = stm_handler_write(stm, key);

    if (ERROR == st || DONE == st)
    {
        socksv5_done(key);
    }
}


static void
socksv5_close(struct selector_key *key)
{
    socks5_destroy(ATTACHMENT(key));
}

static void
socksv5_done(struct selector_key *key)
{
    const int fds[] = {
        ATTACHMENT(key)->client_fd,
        ATTACHMENT(key)->origin_fd,
    };
    for (unsigned i = 0; i < N(fds); i++)
    {
        if (fds[i] != -1)
        {
            if (SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i]))
            {
                abort();
            }
            close(fds[i]);
        }
    }
}
