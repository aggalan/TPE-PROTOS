#include "request.h"
#include "request_parser.h"
#include "../socks5.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/fcntl.h>
#include "../logging/logger.h"

unsigned request_create_connection(struct selector_key *key);
unsigned request_error(SocksClient *data, struct selector_key *key, unsigned status);

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
    socklen_t dest_len = 0; //TODO: check
    int setup_ok = 0; //TODO: check

    //struct sockaddr_storage dest_addr;
    // memset(&dest_addr, 0, sizeof(dest_addr));
    // //TODO: MALLOC?

    if(atyp == IPV4) {
        printf("DEBUG: IPV4\n");
        struct sockaddr_in* addr4 = malloc(sizeof(struct sockaddr_in));
        if(addr4 == NULL) {
            printf("Failed to allocate memory for IPV4 address\n");
            return REQUEST_WRITE;
        }
        // struct sockaddr_in *addr4 = (struct sockaddr_in *)&dest_addr;
        *addr4 = (struct sockaddr_in){
            .sin_family = AF_INET,
            .sin_addr = parser->dst_addr.ipv4,
            .sin_port = htons(parser->dst_port)
        };
        dest_len = sizeof(struct sockaddr_in);
        data->origin_resolution = malloc(sizeof(struct addrinfo));
        if(data->origin_resolution == NULL){
            printf("Failed to allocate memory for origin resolution\n");
            free(addr4);
            return REQUEST_WRITE;
        }
        *data->origin_resolution = (struct addrinfo){
            .ai_family = AF_INET,
            // .ai_socktype = SOCK_STREAM,
            // .ai_protocol = IPPROTO_TCP,
            .ai_addrlen = sizeof(*addr4),
            .ai_addr = (struct sockaddr *)addr4,
        };
        setup_ok = 1;
        char ipstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr4->sin_addr, ipstr, sizeof(ipstr));
        printf("[DEBUG] IPV4 setup ok: %s:%d\n", ipstr, ntohs(addr4->sin_port));
        request_create_connection(key);

    } 
    else if(atyp == IPV6) {
        printf("DEBUG: IPV6\n");
        // struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&dest_addr;
        // *addr6 = (struct sockaddr_in6){
        //     .sin6_family = AF_INET6,
        //     .sin6_addr = parser->dst_addr.ipv6,
        //     .sin6_port = htons(parser->dst_port)
        // };
        // dest_len = sizeof(struct sockaddr_in6);
        // data->origin_resolution = malloc(sizeof(struct addrinfo));
        // if(data->origin_resolution == NULL){
        //     printf("Failed to allocate memory for origin resolution\n");
        //     free(addr6);
        //     return REQUEST_WRITE;
        // }
        // *data->origin_resolution = (struct addrinfo){
        //     .ai_family = AF_INET6,
        //     // .ai_socktype = SOCK_STREAM,
        //     // .ai_protocol = IPPROTO_TCP,
        //     .ai_addrlen = sizeof(struct sockaddr_in6),
        //     .ai_addr = (struct sockaddr *)addr6,
        // };
        // setup_ok = 1;
        // char ipstr[INET6_ADDRSTRLEN];
        // inet_ntop(AF_INET6, &addr6->sin6_addr, ipstr, sizeof(ipstr));
        // printf("[DEBUG] IPV6 setup ok: [%s]:%d\n", ipstr, ntohs(addr6->sin6_port));
        // request_create_connection(key);

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
            // memcpy(&dest_addr, res->ai_addr, res->ai_addrlen);
            // dest_len = res->ai_addrlen;
            setup_ok = 1;
            char ipstr[INET6_ADDRSTRLEN];
            void *addrptr = NULL;
            int port = 0;
            if (res->ai_family == AF_INET) {
                struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
                addrptr = &sin->sin_addr;
                port = ntohs(sin->sin_port);
                inet_ntop(AF_INET, addrptr, ipstr, sizeof(ipstr));
                printf("[DEBUG] DOMAINNAME resolved to: %s:%d\n", ipstr, port);
            } else if (res->ai_family == AF_INET6) {
                struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)res->ai_addr;
                addrptr = &sin6->sin6_addr;
                port = ntohs(sin6->sin6_port);
                inet_ntop(AF_INET6, addrptr, ipstr, sizeof(ipstr));
                printf("[DEBUG] DOMAINNAME resolved to: [%s]:%d\n", ipstr, port);
            }

            data->origin_resolution = res;
            request_create_connection(key);
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
    SocksClient *data = ATTACHMENT(key);
    
}

unsigned request_create_connection(struct selector_key *key) {
    printf("DEBUG: Creating socket...\n");
    SocksClient * data = ATTACHMENT(key);
    printf("Creating socket...\n");
    data->origin_fd = socket(data->origin_resolution->ai_family, SOCK_STREAM | O_NONBLOCK, data->origin_resolution->ai_protocol);
    printf("Socket Correctly!\n");
    if(data->origin_fd < 0){
        data->origin_fd = socket(data->origin_resolution->ai_family, SOCK_STREAM, data->origin_resolution->ai_protocol);
        if(data->origin_fd < 0){
            printf("Failed to create socket from client %d\n",data->origin_fd);
            return ERROR;
        }
    }
    LOG_INFO("request_create_connection: Before selector \n");
    selector_fd_set_nio(data->origin_fd);
    LOG_INFO("request_create_connection: After selector \n");


    if(connect(data->origin_fd, data->origin_resolution->ai_addr, data->origin_resolution->ai_addrlen) == 0 || errno == EINPROGRESS){
        printf("Connected to origin server for client %d\n",data->origin_fd);
        return REQUEST_CONNECTING;
    }

    LOG_INFO("request_create_connection:Connection Failed :/ \n");

    if(data->origin_resolution->ai_next != NULL){
        selector_unregister_fd(key->s, data->origin_fd); //Unregistereamos el Socket fallido del Selector
        close(data->origin_fd); //Lo Cerramos (duh)
        struct addrinfo *next = data->origin_resolution->ai_next; //Preparamos el proximo
        data->origin_resolution->ai_next = NULL; //Lo detacheamos de la Lista para hacerle free
        freeaddrinfo(data->origin_resolution); //Free
        data->origin_resolution = next; //Vamos al proximo
        return request_create_connection(key); //Empezamos again
    }

    return request_error(data, key, -1);
}

//@TODO: Mandar cosas aca para no reutilizar codigo!!!!
unsigned request_error(SocksClient *data, struct selector_key *key, unsigned status){
    ReqParser *parser = &data->client.request_parser;
    parser->status = status;
    parser->state = REQ_ERROR;
    fill_request_answer(parser, &data->write_buffer);
    selector_set_interest_key(key, OP_WRITE);
    return REQUEST_WRITE;
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
        printf("Parsed request successfully\n");
        printf("%s\n", request_to_string(&data->client.request_parser));
        if(!has_request_errors(&data->client.request_parser)) {
            return request_setup(key);
        }
        if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || fill_request_answer(&data->client.request_parser , &data->write_buffer)) {
            printf("No methods allowed or selector error\n");
            return ERROR;
        }
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




