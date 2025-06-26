#include "greeting.h"
#include "../socks5.h"
#include <stdbool.h>
#include <sys/socket.h>

static void
on_hello_method(struct hello_parser *p, const uint8_t method)
{
    uint8_t *selected = p->data;
    if (method == SOCKS_HELLO_NOAUTHENTICATION_REQUIRED)
        *selected = method;
}

static void
greeting_init(const unsigned state, struct selector_key *key)
{
    struct hello_st *d = &ATTACHMENT(key)->client.hello;

    d->rb = &ATTACHMENT(key)->read_buffer;
    d->wb = &ATTACHMENT(key)->write_buffer;

    d->parser.data = &d->method;
    d->parser.on_authentication_method = on_hello_method;
    hello_parser_init(&d->parser);
}

static unsigned hello_process(const struct hello_st *d);

unsigned
greeting_read(struct selector_key *key)
{
    struct hello_st *d = &ATTACHMENT(key)->client.hello;
    uint8_t   *ptr;
    size_t     count;
    ssize_t    n;

    ptr = buffer_write_ptr(d->rb, &count);
    n   = recv(key->fd, ptr, count, 0);
    if (n <= 0)
        return ERROR;

    buffer_write_adv(d->rb, n);

    bool              error = false;
    const enum hello_state st = hello_consume(d->rb, &d->parser, &error);

    if (hello_is_done(st, 0)) {
        if (selector_set_interest_key(key, OP_WRITE) == SELECTOR_SUCCESS)
            return hello_process(d);
        return ERROR;
    }
    return GREETING;
}

unsigned
greeting_write(struct selector_key *key)
{
    struct hello_st *d = &ATTACHMENT(key)->client.hello;
    uint8_t *ptr   = buffer_read_ptr(d->wb, NULL);
    ssize_t  n     = send(key->fd, ptr, buffer_can_read(d->wb), 0);

    if (n <= 0)
        return ERROR;

    buffer_read_adv(d->wb, n);

    if (!buffer_can_read(d->wb)) {
        return DONE;
    }
    return GREETING;
}

static unsigned
hello_process(const struct hello_st *d)
{
    const uint8_t method  = d->method;
    const uint8_t rep     = (method == SOCKS_HELLO_NO_ACCEPTABLE_METHODS)
                            ? 0xFF : 0x00;

    if (hello_marshall(d->wb, rep) == -1)
        return ERROR;

    return HELLO_WRITE;
}

void
greeting_close(const unsigned state, struct selector_key *key)
{
    (void) state;
    (void) key;
}
