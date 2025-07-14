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
#include "metrics/metrics.h"
#include "admin_logs.h"


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
            data->current_addr = data->origin_resolution;
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
            data->current_addr = data->origin_resolution;
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
        socks->current_addr = NULL;
    } else {
    socks->current_addr = socks->origin_resolution; 
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
    LOG_DEBUG("Requesting connecting...\n");
    SocksClient *data = ATTACHMENT(key);
    int error = 0;
    if (getsockopt(data->origin_fd, SOL_SOCKET, SO_ERROR, &error, &(socklen_t){sizeof(int)})) {
        LOG_ERROR("Failed to get socket options for origin fd %d\n", data->origin_fd);
        return request_error(data, key, REQ_ERROR_GENERAL_FAILURE);
    }

    if (error) {
        if (data->origin_resolution->ai_next == NULL) {
            LOG_DEBUG( "Failed to fulfill connection request from client %d", data->client_fd);
            freeaddrinfo(data->origin_resolution);
            data->origin_resolution = NULL;
            data->current_addr = NULL;
            LOG_DEBUG("ERROR: %d\n", error);
            return request_error(data, key, errno_to_req_status(error));
        } else {
            LOG_DEBUG( "Next attempt at connection request from client %d \n", data->client_fd);
            selector_unregister_fd(key->s, data->origin_fd);
            close(data->origin_fd);
            data->current_addr = data->current_addr->ai_next;
            return request_create_connection(key);
        }
    }

    if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || fill_request_answer(&data->client.request_parser, &data->write_buffer, key)) {
        LOG_ERROR("Failed to set interest for origin fd %d in selector\n", data->origin_fd);
        return request_error(data, key, REQ_ERROR_GENERAL_FAILURE);
    }
    LOG_DEBUG("The connection was established!\n");
    return REQUEST_WRITE;
}

ReqStatus errno_to_req_status(int err) {
    switch (err) {
        case ECONNREFUSED:  return REQ_ERROR_CONNECTION_REFUSED;
        case ENETUNREACH:   return REQ_ERROR_NTW_UNREACHABLE;
        case EHOSTUNREACH:  return REQ_ERROR_HOST_UNREACHABLE;
        case ETIMEDOUT:     return REQ_ERROR_TTL_EXPIRED;
        default:            return REQ_ERROR_GENERAL_FAILURE;
    }
}

unsigned request_create_connection(struct selector_key *key) {
    SocksClient * data = ATTACHMENT(key);
    LOG_DEBUG("Creating socket\n");
    char host[NI_MAXHOST], serv[NI_MAXSERV];
    int r = getnameinfo(
        (struct sockaddr *)data->current_addr->ai_addr,
        data->current_addr->ai_addrlen,
        host, sizeof(host),
        serv, sizeof(serv),
        NI_NUMERICHOST | NI_NUMERICSERV
    );
    if (r == 0) {
        LOG_DEBUG("Trying to connect to %s:%s (Client %d)\n", host, serv, data->client_fd);
    } else {
        LOG_DEBUG("Trying to connect to [unknown address] (Client %d)", data->client_fd);
    }
    data->origin_fd = socket(data->origin_resolution->ai_family, SOCK_STREAM | O_NONBLOCK, 0);
    if(data->origin_fd < 0){
        data->origin_fd = socket(data->origin_resolution->ai_family, SOCK_STREAM, 0);
        if(data->origin_fd < 0){
            printf("Failed to create socket from client %d\n",data->origin_fd);
            return ERROR;
        }
    }

    selector_fd_set_nio(data->origin_fd);

    if(connect(data->origin_fd, data->origin_resolution->ai_addr, data->current_addr->ai_addrlen) == 0 || errno == EINPROGRESS){
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
        selector_unregister_fd(key->s, data->origin_fd);
        close(data->origin_fd);
        struct addrinfo* next = data->origin_resolution->ai_next;
        data->origin_resolution->ai_next = NULL;
        //freeaddrinfo(data->origin_resolution);
        data->origin_resolution = next;
        return request_create_connection(key);
    }
    return request_error(data, key, errno_to_req_status(errno));
}

//@TODO: Mandar cosas aca para no reutilizar codigo!!!!
unsigned request_error(SocksClient *data,
                       struct selector_key *key,
                       unsigned status)
{
    ReqParser *parser = &data->client.request_parser;
    if (data->client_username != NULL) {

        char src_ip[INET6_ADDRSTRLEN] = "unknown";
        uint16_t src_port = 0;
        struct sockaddr_storage ss; socklen_t slen = sizeof(ss);
        if (getpeername(data->client_fd, (struct sockaddr *)&ss, &slen) == 0) {
            if (ss.ss_family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *)&ss;
                inet_ntop(AF_INET,  &sa->sin_addr,  src_ip, sizeof(src_ip));
                src_port = ntohs(sa->sin_port);
            } else {
                struct sockaddr_in6 *sa = (struct sockaddr_in6 *)&ss;
                inet_ntop(AF_INET6, &sa->sin6_addr, src_ip, sizeof(src_ip));
                src_port = ntohs(sa->sin6_port);
            }
        }

        char dst[256] = "";
        uint16_t dst_port = ntohs(parser->dst_port);

        switch (parser->atyp) {
            case IPV4:
                inet_ntop(AF_INET,  &parser->dst_addr.ipv4,  dst, sizeof(dst));
                break;
            case IPV6:
                inet_ntop(AF_INET6, &parser->dst_addr.ipv6,  dst, sizeof(dst));
                break;
            case DOMAINNAME:
                memcpy(dst, parser->dst_addr.domainname, parser->dnlen);
                dst[parser->dnlen] = '\0';
                break;
        }

        log_access(data->client_username,
                   src_ip, src_port,
                   dst, dst_port,
                   status);
        data->access_logged = true;
        free(data->client_username);
    }

    parser->status = status;
    parser->state  = REQ_ERROR;
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
    if (!data->access_logged && data->client_username != NULL) {
        char     src_ip[INET6_ADDRSTRLEN] = "unknown";
        uint16_t src_port = 0;
        if (data->client_addr.ss_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)&data->client_addr;
            inet_ntop(AF_INET,  &sa->sin_addr,  src_ip, sizeof(src_ip));
            src_port = ntohs(sa->sin_port);
        } else if (data->client_addr.ss_family == AF_INET6) {
            struct sockaddr_in6 *sa = (struct sockaddr_in6 *)&data->client_addr;
            inet_ntop(AF_INET6, &sa->sin6_addr, src_ip, sizeof(src_ip));
            src_port = ntohs(sa->sin6_port);
        }

        ReqParser *p = &data->client.request_parser;
        char     dst[256] = "";
        uint16_t dst_port = ntohs(p->dst_port);

        switch (p->atyp) {
            case IPV4:
                inet_ntop(AF_INET,  &p->dst_addr.ipv4,  dst, sizeof(dst));
                break;
            case IPV6:
                inet_ntop(AF_INET6, &p->dst_addr.ipv6,  dst, sizeof(dst));
                break;
            case DOMAINNAME:
                memcpy(dst, p->dst_addr.domainname, p->dnlen);
                dst[p->dnlen] = '\0';
                break;
        }

        log_access(data->client_username,
                   src_ip, src_port,
                   dst,    dst_port,
                   0x00);

        data->access_logged = true;
        free(data->client_username);
    }

    if (selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        LOG_ERROR("Failed to set interest for RELAY state");
        return ERROR;
    }

    LOG_DEBUG("Request ended, transitioning to RELAY");
    return RELAY;
}



