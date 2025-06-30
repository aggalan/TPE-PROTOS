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
#include <netdb.h>

void request_init(const unsigned state,struct selector_key * key) {
    printf("Creating request...\n");
    SocksClient *socks = ATTACHMENT(key);
    if (socks == NULL) {
        return;
    }
    init_request_parser(&socks->client.request_parser);
    printf("All request elements created!\n");
}

unsigned request_setup(struct selector_key *key) {
    SocksClient *data = ATTACHMENT(key);
    printf("DEBUG: Setting up request...\n");
    ReqParser *parser = &data->client.request_parser;
    uint8_t atyp = parser->atyp;
    socklen_t dest_len = 0;
    int setup_ok = 0;

    struct sockaddr_storage dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));

    if(atyp == IPV4) {
        printf("DEBUG: IPV4\n");
        struct sockaddr_in *addr4 = (struct sockaddr_in *)&dest_addr;
        addr4->sin_family = AF_INET;
        addr4->sin_addr = parser->dst_addr.ipv4;
        addr4->sin_port = htons(parser->dst_port);
        dest_len = sizeof(struct sockaddr_in);
        setup_ok = 1;
        printf("DEBUG: IPV4 setup ok\n");
    } 
    else if(atyp == IPV6) {
        printf("DEBUG: IPV6\n");
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&dest_addr;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_addr = parser->dst_addr.ipv6;
        addr6->sin6_port = htons(parser->dst_port);
        dest_len = sizeof(struct sockaddr_in6);
        setup_ok = 1;
        printf("DEBUG: IPV6 setup ok\n");
    } 
    else if(atyp == DOMAINNAME) {
        printf("DEBUG: DOMAINNAME\n");
        char host[REQ_MAX_DN_LENGHT + 1] = {0};
        memcpy(host, parser->dst_addr.domainname + 1, parser->dst_addr.domainname[0]);
        host[parser->dst_addr.domainname[0]] = '\0';
        char portstr[6];
        snprintf(portstr, sizeof(portstr), "%u", parser->dst_port);

        struct addrinfo hints = {0}, *res = NULL;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        int err = getaddrinfo(host, portstr, &hints, &res);
        if (err == 0 && res != NULL) {
            memcpy(&dest_addr, res->ai_addr, res->ai_addrlen);
            dest_len = res->ai_addrlen;
            setup_ok = 1;
            printf("DEBUG: DOMAINNAME resolved to sockaddr\n");
            freeaddrinfo(res);
        } else {
            printf("DEBUG: Failed to resolve domain name: %s\n", gai_strerror(err));
        }
    }

    if (!setup_ok) {
        parser->status = REQ_ERROR_ADDRESS_TYPE_NOT_SUPPORTED;
        fill_request_answer(parser, &data->write_buffer);
        selector_set_interest_key(key, OP_WRITE);
        return REQUEST_WRITE;
    }

    return REQUEST_WRITE;
}

void request_connect(struct selector_key *key) {
    printf("DEBUG: Starting connection...\n");
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
        if(!has_request_errors(&data->client.request_parser)) {
            return request_setup(key);
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




