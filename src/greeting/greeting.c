#include "greeting.h"
#include "../socks5.h"
#include <stdbool.h>
#include <sys/socket.h>


static void
greeting_init(const unsigned state, struct selector_key *key)
{
    struct socks5 *client = ATTACHMENT(key);
    if (socks == NULL) {
        return;
    }
    initNegotiationParser(&socks->hello.parser);

}

unsigned
greeting_read(struct selector_key *key)
{
    struct socks5 * data = ATTACHMENT(key);

    size_t read_size;
    ssize_t read_count;
    uint8_t* read_buffer;

    read_buffer = buffer_write_ptr(&data->read_buffer, &read_size);
    read_count = recv(key->fd, read_buffer, read_size, 0);
    if (read_count <= 0) {
        return ERROR;
    }

    buffer_write_adv(&data->read_buffer, read_count);
    negotiationParse(&data->client.hello.parser, &data->read_buffer);
    if (hasNegotiationReadEnded(&data->client.hello.parser)) {
        if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || fillNegotiationAnswer(&data->client.hello.parser, &data->read_buffer)) {
            return ERROR;
        }
        return NEGOTIATION_WRITE;
    }
    return NEGOTIATION_READ;
}

unsigned
greeting_write(struct selector_key *key)
{
    struct socks5* data = ATTACHMENT(key);

    size_t write_size;
    ssize_t write_count;
    uint8_t* write_buffer;

    write_buffer = buffer_read_ptr(&data->write_buffer, &write_size);
    write_count = send(key->fd, write_buffer, write_size, MSG_NOSIGNAL);

    if (write_count <= 0) {
        return ERROR;
    }
    buffer_read_adv(&data->write_buffer, write_count);

    if (buffer_can_read(&data->write_buffer)) {
        return NEGOTIATION_WRITE;
    }

    if (hasNegotiationErrors(&data->client.hello.parser) || selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        return ERROR;
    }

    //ver auth method
}




