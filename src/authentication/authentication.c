#include "authentication.h"
#include "authentication_parser.h"
#include "../socks5/socks5.h"
#include "metrics.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>


void authentication_init(const unsigned state, struct selector_key *key) {
    if(state!=AUTHENTICATION_READ ){
        LOG_ERROR("[Authentication]: Initiated with an invalid state: %u\n", state);
        return;
    }    
    LOG_DEBUG("Creating authentication...\n");
    SocksClient *socks = ATTACHMENT(key);    if (socks == NULL) {
        return;
    }
    init_authentication_parser(&socks->client.authentication_parser);
    LOG_DEBUG("All authentication elements created!\n");
}

unsigned authentication_read(struct selector_key *key) {
    LOG_DEBUG("Starting authentication read...\n");
    SocksClient *data = ATTACHMENT(key);

    size_t read_size;

    uint8_t *read_buffer = buffer_write_ptr(&data->read_buffer, &read_size);
    ssize_t read_count = recv(key->fd, read_buffer, read_size, 0);
    if (read_count <= 0) {
        LOG_ERROR("Authentication_read read error\n");
        return ERROR;
    }

    metrics_add_bytes(read_count);

    buffer_write_adv(&data->read_buffer, read_count);
    authentication_parse(&data->client.authentication_parser, &data->read_buffer);
    data->client_username = strdup(data->client.authentication_parser.uname);
    if (has_authentication_read_ended(&data->client.authentication_parser)) {
        if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || fill_authentication_answer(&data->client.authentication_parser , &data->write_buffer)) {
            LOG_ERROR("Authentication_read selector_set_interest_key failed\n");
            return ERROR;
        }
        LOG_DEBUG("Parsed authentication successfully\n");
        return AUTHENTICATION_WRITE;
    }
    return AUTHENTICATION_READ;
}

unsigned authentication_write(struct selector_key *key) {
    LOG_DEBUG("Starting authentication response...\n");
    SocksClient *data = ATTACHMENT(key);

    size_t   write_size;
    uint8_t *write_ptr = buffer_read_ptr(&data->write_buffer, &write_size);
    ssize_t  n = send(key->fd, write_ptr, write_size, MSG_NOSIGNAL);
    if (n <= 0) {
        LOG_ERROR("Authentication_write send error: %s", strerror(errno));
        perror("authentication_write/send");
        return ERROR;
    }
    metrics_add_bytes(n);
    buffer_read_adv(&data->write_buffer, n);
    LOG_DEBUG("Authentication response sent!\n");
    if (buffer_can_read(&data->write_buffer)) {
        return AUTHENTICATION_WRITE;
    }

    selector_set_interest_key(key, OP_READ);
    LOG_DEBUG("Authentication has ended\n");
    return REQUEST_READ;
}
