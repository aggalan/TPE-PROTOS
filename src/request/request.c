#include "request.h"
#include "request_parser.h"
#include "../socks5/socks5.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/fcntl.h>
#include "../logging/logger.h"
#include "metrics.h"


unsigned request_create_connection(struct selector_key *key);
unsigned request_error(SocksClient *data, struct selector_key *key, unsigned status);

void* request_dns_resolve(void *data);

void request_init(const unsigned state,struct selector_key * key) {
    if(state != REQUEST_READ) {
        LOG_ERROR("Request initialized with an invalid state: %u\n", state);
        return;
    }
    LOG_DEBUG("Creating request...\n");
    SocksClient *socks = ATTACHMENT(key);
    if (socks == NULL) {
        return;
    }
    init_request_parser(&socks->client.request_parser);
    LOG_DEBUG("All request elements created!\n");
}

unsigned request_setup(struct selector_key *key) {
    SocksClient *data = ATTACHMENT(key);
    LOG_DEBUG("Setting up request...");
    ReqParser *parser = &data->client.request_parser;
    uint8_t atyp = parser->atyp;

    struct sockaddr_storage *dest_addr = malloc(sizeof(struct sockaddr_storage));
    if (!dest_addr) {
        LOG_ERROR("Failed to allocate dest_addr");
        return request_error(data, key, REQ_ERROR_GENERAL_FAILURE);
    }
    memset(dest_addr, 0, sizeof(struct sockaddr_storage));
    //data->dest_addr = dest_addr; // Guarda el puntero para liberar luego si corresponde

    switch (atyp) {
        case IPV4: {
            LOG_DEBUG("Setting up IPV4 destination");
            struct sockaddr_in *addr4 = (struct sockaddr_in *)dest_addr;
            *addr4 = (struct sockaddr_in){
                .sin_family = AF_INET,
                .sin_port = htons(parser->dst_port),
                .sin_addr = parser->dst_addr.ipv4
            };
            data->origin_resolution = malloc(sizeof(struct addrinfo));
            if (!data->origin_resolution) {
                LOG_ERROR("Failed to allocate origin_resolution");
                free(dest_addr);
                return request_error(data, key, REQ_ERROR_GENERAL_FAILURE);
            }
            *data->origin_resolution = (struct addrinfo){
                .ai_family = AF_INET,
                .ai_socktype = SOCK_STREAM,
                .ai_protocol = IPPROTO_TCP,
                .ai_addrlen = sizeof(struct sockaddr_in),
                .ai_addr = (struct sockaddr *)addr4,
                .ai_next = NULL
            };
            char ipstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr4->sin_addr, ipstr, sizeof(ipstr));
            LOG_DEBUG("IPV4 setup: [%s]:%d", ipstr, ntohs(addr4->sin_port));
            return request_create_connection(key);
        }
        case IPV6: {
            LOG_DEBUG("Setting up IPV6 destination");
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)dest_addr;
            *addr6 = (struct sockaddr_in6){
                .sin6_family = AF_INET6,
                .sin6_port = htons(parser->dst_port),
                .sin6_addr = parser->dst_addr.ipv6
            };
            data->origin_resolution = malloc(sizeof(struct addrinfo));
            if (!data->origin_resolution) {
                LOG_ERROR("Failed to allocate origin_resolution");
                free(dest_addr);
                return request_error(data, key, REQ_ERROR_GENERAL_FAILURE);
            }
            *data->origin_resolution = (struct addrinfo){
                .ai_family = AF_INET6,
                .ai_socktype = SOCK_STREAM,
                .ai_protocol = IPPROTO_TCP,
                .ai_addrlen = sizeof(struct sockaddr_in6),
                .ai_addr = (struct sockaddr *)addr6,
                .ai_next = NULL
            };
            char ipstr[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &addr6->sin6_addr, ipstr, sizeof(ipstr));
            LOG_DEBUG("IPV6 setup: [%s]:%d", ipstr, ntohs(addr6->sin6_port));
            return request_create_connection(key);
        }
        case DOMAINNAME: {
            LOG_DEBUG("Setting up DOMAINNAME resolution");
            pthread_t dns_thread;
            struct selector_key *key_copy = malloc(sizeof(*key));
            if (!key_copy) {
                LOG_ERROR("Failed to allocate key_copy for DNS thread");
                free(dest_addr);
                return request_error(data, key, REQ_ERROR_GENERAL_FAILURE);
            }
            memcpy(key_copy, key, sizeof(*key));
            // El fd se pausa mientras se resuelve DNS
            if (selector_set_interest_key(key, OP_NOOP) != SELECTOR_SUCCESS) {
                LOG_ERROR("Failed to set interest OP_NOOP for DNS resolve");
                free(key_copy);
                free(dest_addr);
                return ERROR;
            }
            // IMPORTANTE: te lo pido por favor libera el thread de key_copy y dest_addr
            if (pthread_create(&dns_thread, NULL, request_dns_resolve, key_copy) != 0) {
                LOG_ERROR("Failed to create DNS resolution thread");
                free(key_copy);
                free(dest_addr);
                return request_error(data, key, REQ_ERROR_GENERAL_FAILURE);
            }
            return REQUEST_RESOLVE;
            break;
        }
        default: {
            LOG_ERROR("Unsupported address type %d", atyp);
            parser->status = REQ_ERROR_ADDRESS_TYPE_NOT_SUPPORTED;
            fill_request_answer(parser, &data->write_buffer, key);
            selector_set_interest_key(key, OP_WRITE);
            free(dest_addr);
            return REQUEST_WRITE;
        }
    }
    LOG_ERROR("Unexpected path in request_setup (atyp=%d)", atyp);
    return request_error(data, key, REQ_ERROR_GENERAL_FAILURE);
}

void* request_dns_resolve(void *data) {
    LOG_DEBUG("Resolving DNS...\n");

    struct selector_key *key = (struct selector_key *)data;
    SocksClient *socks = ATTACHMENT(key);

    pthread_detach(pthread_self());

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE,
        .ai_protocol = 0,
        .ai_canonname = NULL,
        .ai_addr = NULL,
        .ai_next = NULL,
    };

    char service[7];
    sprintf(service, "%d", (int)socks->client.request_parser.dst_port);
    LOG_DEBUG("Resolving %s:%s\n", socks->client.request_parser.dst_addr.domainname, service);
    int err = getaddrinfo((char *)socks->client.request_parser.dst_addr.domainname, service, &hints, &socks->origin_resolution);
    LOG_DEBUG("ERROR: %d\n", err);
    if (err != 0) {
        LOG_ERROR("DNS resolution failed for %s:%d: %s", socks->client.request_parser.dst_addr.domainname, socks->client.request_parser.dst_port, gai_strerror(err));
        socks->origin_resolution = NULL;
    }
    selector_notify_block(key->s, key->fd);

    LOG_DEBUG("DNS resolve done");
    free(key);
    return NULL;
}

unsigned request_resolve_done(struct selector_key *key) {
    LOG_DEBUG("[resolve done]:     Resolving request...\n");
    SocksClient *data = ATTACHMENT(key);
    if (data->origin_resolution == NULL) {
        return request_error(data, key, REQ_ERROR_HOST_UNREACHABLE);
    }

    return request_create_connection(key);
}

void request_connecting_init(const unsigned state,struct selector_key *key) {
    LOG_DEBUG("Starting connection...\n");
    if(key == NULL) {
        LOG_ERROR("Key is NULL in request_connecting_init\n");
        return;
    }
    if(state!=REQUEST_CONNECTING && state!=REQUEST_RESOLVE) {
        LOG_ERROR("Request connecting initialized with an invalid state: %u\n", state);
        return;
    }
}

unsigned request_connecting(struct selector_key *key) {
    SocksClient *data = ATTACHMENT(key);
    int error = 0;
    if (getsockopt(data->origin_fd, SOL_SOCKET, SO_ERROR, &error, &(socklen_t){sizeof(int)})) {
        LOG_ERROR("Failed to get socket options for origin fd %d\n", data->origin_fd);
        return request_error(data, key, REQ_ERROR_GENERAL_FAILURE);
    }

    //mandar la respuesta
    if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || fill_request_answer(&data->client.request_parser, &data->write_buffer, key)) {
        LOG_ERROR("Failed to set interest for origin fd %d in selector\n", data->origin_fd);
        return request_error(data, key, REQ_ERROR_GENERAL_FAILURE);
    }
    LOG_INFO("CONNECTING...\n");
    LOG_DEBUG("The connection was established!\n");
    return REQUEST_WRITE;
}

unsigned request_create_connection(struct selector_key *key) {
    SocksClient * data = ATTACHMENT(key);
    LOG_DEBUG("Creating socket\n");
    data->origin_fd = socket(data->origin_resolution->ai_family, SOCK_STREAM | O_NONBLOCK, 0);
    if(data->origin_fd < 0){
        data->origin_fd = socket(data->origin_resolution->ai_family, SOCK_STREAM, 0);
        if(data->origin_fd < 0){
            printf("Failed to create socket from client %d\n",data->origin_fd);
            return ERROR;
        }
    }

    selector_fd_set_nio(data->origin_fd);

    LOG_DEBUG("Socket created!\n");

    if(connect(data->origin_fd, data->origin_resolution->ai_addr, data->origin_resolution->ai_addrlen) == 0 || errno == EINPROGRESS){
        if (selector_register(key->s, data->origin_fd, get_fd_handler() , OP_WRITE, data) != SELECTOR_SUCCESS) {
            LOG_ERROR("Failed to register origin fd %d in selector\n", data->origin_fd);
            close(data->origin_fd);
            return ERROR;
        }
        if (selector_set_interest_key(key,OP_NOOP) != SELECTOR_SUCCESS) {
            LOG_ERROR("Failed to set interest for origin fd %d in selector\n", data->origin_fd);
            return ERROR;
        }

        LOG_DEBUG("Attemping connection with Client Number %d\n",data->client_fd);
        return REQUEST_CONNECTING;
    }

    if(data->origin_resolution->ai_next != NULL){
        selector_unregister_fd(key->s, data->origin_fd); //Unregistereamos el Socket fallido del Selector
        close(data->origin_fd); //Lo Cerramos (duh)
        struct addrinfo *next = data->origin_resolution->ai_next; //Preparamos el proximo
        data->origin_resolution->ai_next = NULL; //Lo detacheamos de la Lista para hacerle free
        freeaddrinfo(data->origin_resolution); //Free
        data->origin_resolution = next;
        return request_create_connection(key); //Empezamos again
    }

    return request_error(data, key, -1);
}

//@TODO: Mandar cosas aca para no reutilizar codigo!!!!
unsigned request_error(SocksClient *data, struct selector_key *key, unsigned status){
    ReqParser *parser = &data->client.request_parser;
    parser->status = status;
    parser->state = REQ_ERROR;
    fill_request_answer(parser, &data->write_buffer, key);
    selector_set_interest_key(key, OP_WRITE);
    return REQUEST_WRITE;
}

unsigned request_read(struct selector_key *key) {
    LOG_DEBUG("Starting request read...\n");
    SocksClient * data = ATTACHMENT(key);

    size_t read_size;

    uint8_t *read_buffer = buffer_write_ptr(&data->read_buffer, &read_size);
    ssize_t read_count = recv(key->fd, read_buffer, read_size, 0);

    if (read_count == 0) {
        LOG_DEBUG("Client closed the connection\n");
        return DONE; // Client closed connection
    }
    if (read_count < 0) {
        LOG_ERROR("recv error: %s", strerror(errno));
        return ERROR; // Error in recv
    }

    metrics_add_bytes(read_count);

    buffer_write_adv(&data->read_buffer, read_count);
    request_parse(&data->client.request_parser, &data->read_buffer);
    if (has_request_read_ended(&data->client.request_parser)) {
        LOG_DEBUG("Parsed request successfully\n");
        LOG_DEBUG("%s\n", request_to_string(&data->client.request_parser));
        if(!has_request_errors(&data->client.request_parser)) {
            return request_setup(key);
        }
        if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || fill_request_answer(&data->client.request_parser , &data->write_buffer, key)) {
            printf("No methods allowed or selector error\n");
            return ERROR;
        }
        return REQUEST_WRITE;
    }
    return REQUEST_READ;
}

unsigned request_write(struct selector_key *key) {
    LOG_DEBUG("Starting request response...\n");
    SocksClient* data = ATTACHMENT(key);

    size_t write_size;
    uint8_t * write_buffer = buffer_read_ptr(&data->write_buffer, &write_size);
    ssize_t write_count = send(data->client_fd, write_buffer, write_size, MSG_NOSIGNAL);
    if (write_count < 0) {
        printf("request response send error\n");
        return ERROR;
    }
    metrics_add_bytes(write_count);
    LOG_DEBUG("Request response sent!\n");

    buffer_read_adv(&data->write_buffer, write_count);

    if (buffer_can_read(&data->write_buffer)) {
        return REQUEST_WRITE;
    }

    // ADD THIS VALIDATION BEFORE TRANSITIONING TO RELAY
    if (data->origin_fd == -1) {
        LOG_ERROR("Cannot transition to RELAY: origin connection not established");
        return ERROR;
    }

    if (has_request_errors(&data->client.request_parser)) {
        LOG_ERROR("Request has errors, cannot transition to RELAY");
        return ERROR;
    }

    if (selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        LOG_ERROR("Failed to set interest for RELAY state");
        return ERROR;
    }

    LOG_DEBUG("Request ended, transitioning to RELAY");
    return RELAY;
}



