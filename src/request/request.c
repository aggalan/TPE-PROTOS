#include "request.h"
#include "request_parser.h"
#include "../socks5.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

void request_init(const unsigned state,struct selector_key * key) {
    printf("Creating request...\n");
    SocksClient *socks = ATTACHMENT(key);
    if (socks == NULL) {
        return;
    }
    init_request_parser(&socks->client.request_parser);
    printf("All request elements created!\n");
}

unsigned request_read(struct selector_key *key) {
    printf("Started reading request\n");
    SocksClient * data = ATTACHMENT(key);

    size_t read_size;

    uint8_t *read_buffer = buffer_write_ptr(&data->read_buffer, &read_size);
    ssize_t read_count = recv(key->fd, read_buffer, read_size, 0);
    if (read_count <= 0) {
        printf("Greeting_read read error\n");
        return ERROR;
    }

    buffer_write_adv(&data->read_buffer, read_count);
    request_parse(&data->client.request_parser, &data->read_buffer);
    if (has_request_read_ended(&data->client.request_parser)) {
        printf("%s\n", request_to_string(&data->client.request_parser));
        if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || fill_request_answer(&data->client.request_parser , &data->write_buffer)) {
            printf("No methods allowed or selector error\n");
            return ERROR;
        }
        printf("Parsed request successfully\n");
        return REQUEST_WRITE;
    }
    return REQUEST_READ;
}

unsigned request_write(struct selector_key *key) {
    printf("Started request response\n");
    SocksClient* data = ATTACHMENT(key);

    size_t write_size;

    uint8_t * write_buffer = buffer_read_ptr(&data->write_buffer, &write_size);
    ssize_t write_count = send(key->fd, write_buffer, write_size, MSG_NOSIGNAL);

    if (write_count < 0) {
        printf("request response send error\n");
        return ERROR;
    }

    printf("Response sent!\n");

    buffer_read_adv(&data->write_buffer, write_count);

    if (buffer_can_read(&data->write_buffer)) {
        return REQUEST_WRITE;
    }

    if (has_request_errors(&data->client.request_parser) || selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        return ERROR;
    }

    printf("Request ended: OK\n");
    return DONE;
}




