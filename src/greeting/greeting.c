#include "greeting.h"
#include "../socks5.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>


void greeting_init(const unsigned state,struct selector_key *key) {
    printf("Greeting_init\n");
    SocksClient *socks = ATTACHMENT(key);
    if (socks == NULL) {
        return;
    }
    initNegotiationParser(&socks->client.negotiation_parser);

}

unsigned greeting_read(struct selector_key *key) {
    printf("Greeting_read\n");
    struct socks5 * data = ATTACHMENT(key);

    size_t read_size;

    uint8_t *read_buffer = buffer_write_ptr(&data->read_buffer, &read_size);
    ssize_t read_count = recv(key->fd, read_buffer, read_size, 0);
    if (read_count <= 0) {
        printf("Greeting_read read error\n");
        return ERROR;
    }

    buffer_write_adv(&data->read_buffer, read_count);
    negotiationParse(&data->client.negotiation_parser, &data->read_buffer);
    if (hasNegotiationReadEnded(&data->client.negotiation_parser)) {
        if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || fillNegotiationAnswer(&data->client.negotiation_parser , &data->read_buffer)) {
            printf("Greeting_read selector_set_interest_key failed\n");
            return ERROR;
        }
        printf("negotiation answer\n");
        return NEGOTIATION_WRITE;
    }
    return NEGOTIATION_READ;
}

unsigned greeting_write(struct selector_key *key) {
    printf("started negotiation writing\n");
    SocksClient* data = ATTACHMENT(key);

    size_t write_size;

    char response[2] = {SOCKS_VERSION, data->client.negotiation_parser.auth_method}; // Respuesta de saludo con autenticación no requerida
    send(data->origin_fd, response, sizeof(response), MSG_DONTWAIT);

    uint8_t * write_buffer = buffer_read_ptr(&data->write_buffer, &write_size);
    ssize_t write_count = send(key->fd, write_buffer, write_size, MSG_NOSIGNAL);

    if (write_count < 0) {
        printf("Greeting_write send error\n");
        return ERROR;
    }
    buffer_read_adv(&data->write_buffer, write_count);

    if (buffer_can_read(&data->write_buffer)) {
        return NEGOTIATION_WRITE;
    }

    if (hasNegotiationErrors(&data->client.negotiation_parser) || selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        return ERROR;
    }



    //ver auth method

    return OK;
}




