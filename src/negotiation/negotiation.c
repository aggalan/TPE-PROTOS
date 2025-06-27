#include "negotiation.h"
#include "../socks5.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>


void negotiation_init(const unsigned state,struct selector_key *key) {
    printf("Creating negotiation...\n");
    SocksClient *socks = ATTACHMENT(key);
    if (socks == NULL) {
        return;
    }
    init_negotiation_parser(&socks->client.negotiation_parser);
    printf("All negotiation elements created!\n");

}

unsigned negotiation_read(struct selector_key *key) {
    printf("Started reading negotiation\n");
    SocksClient * data = ATTACHMENT(key);

    size_t read_size;

    uint8_t *read_buffer = buffer_write_ptr(&data->read_buffer, &read_size);
    ssize_t read_count = recv(key->fd, read_buffer, read_size, 0);
    if (read_count <= 0) {
        printf("Greeting_read read error\n");
        return ERROR;
    }

    buffer_write_adv(&data->read_buffer, read_count);
    negotiation_parse(&data->client.negotiation_parser, &data->read_buffer);
    if (has_negotiation_read_ended(&data->client.negotiation_parser)) {
        if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || fill_negotiation_answer(&data->client.negotiation_parser , &data->write_buffer)) {
            printf("No methods allowed or selector error\n");
            return ERROR;
        }
        printf("Parsed negotiation successfully\n");
        return NEGOTIATION_WRITE;
    }
    return NEGOTIATION_READ;
}

unsigned negotiation_write(struct selector_key *key) {
    printf("Started negotiation response\n");
    SocksClient* data = ATTACHMENT(key);

    size_t write_size;

    uint8_t * write_buffer = buffer_read_ptr(&data->write_buffer, &write_size);
    ssize_t write_count = send(key->fd, write_buffer, write_size, MSG_NOSIGNAL); //NOWAIT?

    if (write_count < 0) {
        printf("Greeting_write send error\n");
        return ERROR;
    }

    printf("Response sent!\n");

    buffer_read_adv(&data->write_buffer, write_count);

    if (buffer_can_read(&data->write_buffer)) {
        return NEGOTIATION_WRITE;
    }

    if (has_negotiation_errors(&data->client.negotiation_parser) || selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        return ERROR;
    }

    printf("Negotiation ended: OK\n");

    if (USER_PASS == data->client.negotiation_parser.auth_method) {
        printf("User has selected authentication\n");
        return AUTHENTICATION_READ;
    }

    printf("User has selected NO authentication\n");

    //return REQUEST_READ
    return DONE;
}




