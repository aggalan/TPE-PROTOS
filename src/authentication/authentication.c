#include "authentication.h"
#include "authentication_parser.h"
#include "../socks5.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>


void authentication_init(const unsigned state, struct selector_key *key) {
    printf("Creating authentication... [state:%d]\n",state);
    SocksClient *socks = ATTACHMENT(key);
    if (socks == NULL) {
        return;
    }
    init_authentication_parser(&socks->client.authentication_parser);
    printf("All authentication elements created!\n");
}

unsigned authentication_read(struct selector_key *key) {
    printf("Started reading authentication\n");
    SocksClient *data = ATTACHMENT(key);

    size_t read_size;

    uint8_t *read_buffer = buffer_write_ptr(&data->read_buffer, &read_size);
    ssize_t read_count = recv(key->fd, read_buffer, read_size, 0);
    if (read_count <= 0) {
        printf("Authentication_read read error\n");
        return ERROR;
    }

    buffer_write_adv(&data->read_buffer, read_count);
    authentication_parse(&data->client.authentication_parser, &data->read_buffer);
    if (has_authentication_read_ended(&data->client.authentication_parser)) {
        if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || fill_authentication_answer(&data->client.authentication_parser , &data->write_buffer)) {
            printf("Authentication_read selector_set_interest_key failed\n");
            return ERROR;
        }
        printf("Parsed authentication successfully\n");
        return AUTHENTICATION_WRITE;
    }
    return AUTHENTICATION_READ;
}

unsigned authentication_write(struct selector_key *key) {
    printf("Started authentication response\n");
    SocksClient *data = ATTACHMENT(key);

    size_t   write_size;
    uint8_t *write_ptr = buffer_read_ptr(&data->write_buffer, &write_size);
    ssize_t  n = send(key->fd, write_ptr, write_size, MSG_NOSIGNAL);
    if (n <= 0) {
        perror("authentication_write/send");
        return ERROR;
    }
    buffer_read_adv(&data->write_buffer, n);

    if (buffer_can_read(&data->write_buffer)) {
        return AUTHENTICATION_WRITE;
    }

    selector_set_interest_key(key, OP_READ);
    printf("Response sent!\n");
    return REQUEST_READ;
}
