#include "negotiation.h"
#include "../socks5/socks5.h"
#include "../metrics/metrics.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>


void negotiation_init(const unsigned state,struct selector_key *key) {
    if(state!=(unsigned int)0){
        LOG_ERROR("[Negotiation] Initiated with an invalid state: %u\n", state);
        return;
    }
    LOG_DEBUG("Creating negotiation...\n");
    SocksClient *socks = ATTACHMENT(key);
    if (socks == NULL) {
        return;
    }
    init_negotiation_parser(&socks->client.negotiation_parser);
    LOG_DEBUG("All negotiation elements created!\n");

}

unsigned negotiation_read(struct selector_key *key) {
    LOG_DEBUG("Started reading negotiation...\n");
    SocksClient * data = ATTACHMENT(key);

    size_t read_size;

    uint8_t *read_buffer = buffer_write_ptr(&data->read_buffer, &read_size);
    ssize_t read_count = recv(key->fd, read_buffer, read_size, 0);
    if (read_count < 0) {
        LOG_ERROR("recv error: %s", strerror(errno));
        return ERROR;
    } if (read_count == 0) {
        LOG_DEBUG("Client closed the connection");
        return ERROR;
    }
    metrics_add_bytes(read_count);

    buffer_write_adv(&data->read_buffer, read_count);
    negotiation_parse(&data->client.negotiation_parser, &data->read_buffer);
    if (has_negotiation_read_ended(&data->client.negotiation_parser)) {
        if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || fill_negotiation_answer(&data->client.negotiation_parser , &data->write_buffer)) {
            LOG_ERROR("No methods allowed or selector error\n");
            return ERROR;
        }
        LOG_DEBUG("Negotiation parsed successfully\n");
        return NEGOTIATION_WRITE;
    }
    return NEGOTIATION_READ;
}

unsigned negotiation_write(struct selector_key *key) {
    LOG_DEBUG("Starting negotiation response...\n");
    SocksClient* data = ATTACHMENT(key);

    size_t write_size;

    uint8_t * write_buffer = buffer_read_ptr(&data->write_buffer, &write_size);
    ssize_t write_count = send(key->fd, write_buffer, write_size, MSG_NOSIGNAL); //NOWAIT?

    if (write_count < 0) {
        printf("Greeting_write send error\n");
        return ERROR;
    }

    metrics_add_bytes(write_count);

    LOG_DEBUG("Negotiation response sent!\n");

    buffer_read_adv(&data->write_buffer, write_count);

    if (buffer_can_read(&data->write_buffer)) {
        return NEGOTIATION_WRITE;
    }

    if (has_negotiation_errors(&data->client.negotiation_parser) || selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        return ERROR;
    }

    LOG_DEBUG("Negotiation ended\n");

    if (USER_PASS == data->client.negotiation_parser.auth_method) {
        LOG_DEBUG("User has selected USER_PASS authentication\n");
        return AUTHENTICATION_READ;
    }

    LOG_DEBUG("User has selected NO_AUTH authentication\n");

    return REQUEST_READ;
}




