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
#include "../selector/selector.h"
#include "tests.h"
#include "socks5.h"
#include "../buffer/buffer.h"
#include "../stm/stm.h"
#include "../parser/parser.h"
#include "../metrics/metrics.h"
#include "../negotiation/negotiation.h"
#include "../authentication/authentication.h"
#include "../request/request.h"
#include "../args/args.h"


#define FD_UPPER_LIMIT 1024
#define ERROR_CODE -1


static void socksv5_read(struct selector_key *key);
static void socksv5_write(struct selector_key *key);
static void socksv5_close(struct selector_key *key);
static void socksv5_block(struct selector_key *key);


static const struct fd_handler socks5_handler = {
    .handle_read = socksv5_read,
    .handle_write = socksv5_write,
    .handle_close = socksv5_close,
    .handle_block = socksv5_block,
};


const fd_handler * get_fd_handler() {
    return &socks5_handler;
}


void socksv5_done(const unsigned state, struct selector_key * key);
void socksv5_error(const unsigned state, struct selector_key * key);

static const struct state_definition client_actions[] = {
    /* === Negotiation phase === */
    {
        .state        = NEGOTIATION_READ,
        .on_arrival   = negotiation_init,
        .on_read_ready  = negotiation_read,
    },
    {
        .state        = NEGOTIATION_WRITE,
        .on_write_ready = negotiation_write,
    },
        /* === Authentication phase === */
    {
        .state        = AUTHENTICATION_READ,
        .on_arrival   = authentication_init,
        .on_read_ready  = authentication_read,
    },
    {
        .state        = AUTHENTICATION_WRITE,
        .on_write_ready = authentication_write,
    },
    /* === Request phase === */
    {
        .state = REQUEST_READ,
        .on_arrival = request_init,
        .on_read_ready = request_read,
    },
    {
        .state = REQUEST_WRITE,
        .on_write_ready = request_write,
    },
    {
        .state = REQUEST_CONNECTING,
        .on_arrival = request_connecting_init,
        .on_read_ready = request_connecting,
        .on_block_ready = request_connecting,
        .on_write_ready = request_connecting,
    },
    {
        .state = REQUEST_RESOLVE,
        .on_block_ready = request_resolve_done,
    },
    {
        .state = RELAY,
        .on_arrival = relay_init,
        .on_read_ready = relay_read,
        .on_write_ready = relay_write,
        //.on_departure = relay_close,
    },
    /* === Terminal states === */
    {
        .state        = DONE,
        .on_arrival = socksv5_done,
    },
    {
        .state        = ERROR,
        .on_arrival = socksv5_error,
    }
};



SocksClient *socks5_new(const int client_fd, const struct sockaddr_storage *client_addr,const socklen_t client_addr_len){
    SocksClient * ret = calloc(1, sizeof(SocksClient));
    if (ret == NULL) goto fail;
    ret->client_fd = client_fd;
    ret->origin_fd = ERROR_CODE;
    ret->client_addr = *client_addr;
    ret->client_addr_len = client_addr_len;
    ret->closed = false;
    buffer_init(&ret->read_buffer, socks5args.buffer_size, ret->read_buffer_space);
    buffer_init(&ret->write_buffer, socks5args.buffer_size, ret->write_buffer_space);

    //State Machine Set up
    ret->stm.initial = NEGOTIATION_READ;
    ret->stm.states = client_actions;
    ret->stm.max_state = ERROR;
    ret->closed = false;
    stm_init(&(ret->stm));
    return ret;

fail:
    fprintf(stderr,"couldn't allocate memory to create client[fd=%d]",ret->client_fd);
    if (ret != NULL)
    {
        free(ret);
    }
    return NULL;
}


void socksv5_passive_accept(struct selector_key *key){
    LOG_DEBUG("New connection received\n");
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    struct socks5 *state = NULL;
    const int client = accept(key->fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client < 0)
    {
        fprintf(stderr,"Socks socket: accept() has returned a negative value: %d\n", client);
        goto fail;
    }
    if (get_concurrent_connections() > 500) {
        fprintf(stderr, "Connection limit reached\n");
        goto fail;
    }
    if (client >= FD_UPPER_LIMIT)
    {
        close(client);
        fprintf(stderr,"Socks fd was too high\n");   
        goto fail;
    }
    if (selector_fd_set_nio(client) == ERROR_CODE)
    {
        fprintf(stderr, "Couldn't set socket non-blocking\n");
        goto fail;
    }
    state = socks5_new(client, &client_addr, client_addr_len);
    LOG_DEBUG("Accepted new connection!\n");
    if (state == NULL)
    {
        goto fail;
    }
    metrics_new_connection();

    if (SELECTOR_SUCCESS != selector_register(key->s, client, &socks5_handler,OP_READ, state))
    {
        fprintf(stderr, "Couldn't register the socket\n");
        goto fail;
    }
    return;

fail:
    if (client != ERROR_CODE)
    {
        close(client);
    }
}


static void socksv5_read(struct selector_key *key)
{
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum socks_v5state st = stm_handler_read(stm, key);

    if (ERROR == st || DONE == st)
    {
        _closeConnection(key);
    }
}
static void socksv5_block(struct selector_key *key) {
    struct state_machine* stm = &ATTACHMENT(key)->stm;
    const enum socks_v5state st = stm_handler_block(stm, key);
    if (st == ERROR || st == DONE) {
        _closeConnection(key);
    }
}

static void
socksv5_write(struct selector_key *key){
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum socks_v5state st = stm_handler_write(stm, key);

    if (ERROR == st || DONE == st)
    {
        _closeConnection(key);
    }
}


static void socksv5_close(struct selector_key *key)
{ 
    struct state_machine* stm = &ATTACHMENT(key)->stm;
    stm_handler_close(stm, key);
}

void _closeConnection(struct selector_key *key)
{
    SocksClient* data = ATTACHMENT(key);

    if (data->closed) return;

    data->closed = true;
    LOG_DEBUG("Socks client %d disconected",key->fd);

    int clientSocket = data->client_fd;
    int serverSocket = data->origin_fd;

    if (serverSocket != ERROR_CODE)
    {
        selector_unregister_fd(key->s,serverSocket);
        close(serverSocket);
    }
    if (clientSocket != ERROR_CODE)
    {
        selector_unregister_fd(key->s,clientSocket);
        close(clientSocket);
    }

    if (data->origin_resolution != NULL)
    {
        freeaddrinfo(data->origin_resolution);
        data->origin_resolution = NULL;
    }
    free(data);

}

void socksv5_done(const unsigned state, struct selector_key * key)
{
    if(state!=DONE){
        LOG_DEBUG("socksv5_done called with unexpected state: %u\n", state);
    }
    LOG_DEBUG("Socks DONE...\n");
    const int fds[] = {
        ATTACHMENT(key)->client_fd,
        ATTACHMENT(key)->origin_fd,
    };
    metrics_closed_connection();
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

void socksv5_error(const unsigned state, struct selector_key * key){
    (void)state;
    LOG_DEBUG("Socks error (state: %u)\n", state);
    metrics_closed_connection();
    
    if (key != NULL && ATTACHMENT(key) != NULL) {
        SocksClient *data = ATTACHMENT(key);
        data->closed = true;
        
        if (data->client_fd != -1) {
            selector_unregister_fd(key->s, data->client_fd);
            close(data->client_fd);
        }
        if (data->origin_fd != -1) {
            selector_unregister_fd(key->s, data->origin_fd);
            close(data->origin_fd);
        }
        
        if (data->origin_resolution != NULL) {
            freeaddrinfo(data->origin_resolution);
            data->origin_resolution = NULL;
        }
        
        free(data);
    }
}
