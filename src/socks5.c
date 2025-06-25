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

/** obtiene el struct (socks5 *) desde la llave de selección  */
#define ATTACHMENT(key) ((struct socks5 *)(key)->data)

/** maquina de estados general */
enum socks_v5state
{
    /**
     * recibe el mensaje `hello` del cliente, y lo procesa
     *
     * Intereses:
     *     - OP_READ sobre client_fd
     *
     * Transiciones:
     *   - HELLO_READ  mientras el mensaje no esté completo
     *   - HELLO_WRITE cuando está completo
     *   - ERROR       ante cualquier error (IO/parseo)
     */
    HELLO_READ,

    /**
     * envía la respuesta del `hello' al cliente.
     *
     * Intereses:
     *     - OP_WRITE sobre client_fd
     *
     * Transiciones:
     *   - HELLO_WRITE  mientras queden bytes por enviar
     *   - REQUEST_READ cuando se enviaron todos los bytes
     *   - ERROR        ante cualquier error (IO/parseo)
     */
    HELLO_WRITE,
    ECHO_READ,
    ECHO_WRITE,

    // estados terminales
    DONE,
    ERROR,
};

struct echo_st
{
    char *buffer;
    unsigned int buffer_length;
};
struct hello_st
{
    /** buffer utilizado para I/O */
    buffer *rb, *wb;
    //struct hello_parser parser;
    /** el método de autenticación seleccionado */
    uint8_t method;
};

struct socks5 {
    int client_fd;
    int origin_fd;

    /** buffers compartidos por los estados */
    buffer read_buffer;
    buffer write_buffer;

    /** máquina de estados */
    struct state_machine stm;

    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;

    /** estados para el client_fd */
    union {
        struct hello_st hello;
        struct echo_st echo;
        // otros estados que quieras agregar luego...
    } client;

    union {
        // struct connecting conn;
        // struct copy copy;
    } orig;
};


/**
 * destruye un  `struct socks5', tiene en cuenta las referencias
 * y el pool de objetos.
 */

/* declaración forward de los handlers de selección de una conexión
 * establecida entre un cliente y el proxy.
 */
static void socksv5_read(struct selector_key *key);
static void socksv5_write(struct selector_key *key);
// static void socksv5_block(struct selector_key *key);
static void socksv5_close(struct selector_key *key);
static const struct fd_handler socks5_handler = {
    .handle_read = socksv5_read,
    .handle_write = socksv5_write,
    .handle_close = socksv5_close,
    //   .handle_block = socksv5_block,
};

/** Constructor del estado para una nueva conexión */
struct socks5 *socks5_new(const int client_fd)
{
    struct socks5 *ret = calloc(1, sizeof(*ret));
    if (ret == NULL)
    {
        goto fail;
    }

    ret->client_fd = client_fd;
    ret->origin_fd = -1;

    // inicializa buffers de lectura y escritura
    static uint8_t r_buffer[BUFFER_SIZE], w_buffer[BUFFER_SIZE];
    buffer_init(&ret->read_buffer, BUFFER_SIZE, r_buffer);
    buffer_init(&ret->write_buffer, BUFFER_SIZE, w_buffer);

    // inicializa la máquina de estados
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
    state = socks5_new(client);
    if (state == NULL)
    {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberó alguna conexión.
        goto fail;
    }
    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;

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

////////////////////////////////////////////////////////////////////////////////
// HELLO
////////////////////////////////////////////////////////////////////////////////

/** callback del parser utilizado en `read_hello' */
static void
on_hello_method(struct hello_parser *p, const uint8_t method)
{
   // uint8_t *selected = p->data;

    // if (SOCKS_HELLO_NOAUTHENTICATION_REQUIRED == method)
    // {
    //     *selected = method;
    // }
}

/** inicializa las variables de los estados HELLO_… */
static void
hello_read_init(const unsigned state, struct selector_key *key)
{
    struct hello_st *d = &ATTACHMENT(key)->client.hello;

    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);
   // d->parser.data = &d->method;
   // d->parser.on_authentication_method = on_hello_method, hello_parser_init(&d->parser);
}

static unsigned
hello_process(const struct hello_st *d);

/** lee todos los bytes del mensaje de tipo `hello' y inicia su proceso */
static unsigned
hello_read(struct selector_key *key)
{
    struct hello_st *d = &ATTACHMENT(key)->client.hello;
    unsigned ret = HELLO_READ;
    bool error = false;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    ptr = buffer_write_ptr(d->rb, &count);
    n = recv(key->fd, ptr, count, 0);
    if (n > 0)
    {
        // buffer_write_adv(d->rb, n);
        // const enum hello_state st = hello_consume(d->rb, &d->parser, &error);
        // if (hello_is_done(st, 0))
        // {
        //     if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE))
        //     {
        //         ret = hello_process(d);
        //     }
        //     else
        //     {
        //         ret = ERROR;
        //     }
        // }
    }
    else
    {
        ret = ERROR;
    }

    return error ? ERROR : ret;
}

/** procesamiento del mensaje `hello' */
static unsigned
hello_process(const struct hello_st *d)
{
    unsigned ret = HELLO_WRITE;

    // uint8_t m = d->method;
    // const uint8_t r = (m == SOCKS_HELLO_NO_ACCEPTABLE_METHODS) ? 0xFF : 0x00;
    // if (-1 == hello_marshall(d->wb, r))
    // {
    //     ret = ERROR;
    // }
    // if (SOCKS_HELLO_NO_ACCEPTABLE_METHODS == m)
    // {
    //     ret = ERROR;
    // }
    return ret;
}

/** definición de handlers para cada estado */
static const struct state_definition client_statbl[] = {
    {
        .state = HELLO_READ,
        .on_arrival = hello_read_init,
//        .on_departure = hello_read_close,
        .on_read_ready = hello_read,
    },
};
    ///////////////////////////////////////////////////////////////////////////////
    // Handlers top level de la conexión pasiva.
    // son los que emiten los eventos a la maquina de estados.

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
socksv5_block(struct selector_key *key)
{
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum socks_v5state st = stm_handler_block(stm, key);

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
