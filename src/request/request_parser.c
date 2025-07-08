#include "request_parser.h"
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "../socks5/socks5.h"
#include "../logging/logger.h"

void init_request_parser(ReqParser *p) {
    if (p == NULL) return;
    memset(p, 0, sizeof(ReqParser));
    p->state = REQ_VERSION;
    p->status = REQ_SUCCEDED;
    p->buf_idx = 0;
}

ReqState request_parse(ReqParser* p, buffer* b) {
    LOG_DEBUG("Starting request parse...\n");
    if (p == NULL || b == NULL) return REQ_ERROR;
    while (buffer_can_read(b)) {
        uint8_t c = buffer_read(b);

        switch (p->state) {
            case REQ_VERSION:
                if (c == SOCKS_VERSION) {
                    p->version = c;
                    p->state = REQ_CMD;
                } else {
                    p->state = REQ_ERROR;
                    return p->state;
                }
                break;

            case REQ_CMD:
                p->cmd = c;
                p->state = REQ_RSV;
                break;

            case REQ_RSV:
                if (c == 0x00) {
                    p->state = REQ_ATYP;
                } else {
                    p->state = REQ_ERROR;
                    return p->state;
                }
                break;

            case REQ_ATYP:
                p->atyp = c;
                switch (p->atyp) {
                    case IPV4:
                        p->state = REQ_DST_ADDR;
                        break;
                    case DOMAINNAME:
                        p->state = REQ_DNLEN;
                        break;
                    case IPV6:
                        p->state = REQ_DST_ADDR;
                        break;
                    default:
                        p->state = REQ_ERROR;
                        return p->state;
                }
                break;

            case REQ_DNLEN:
                if (c == 0 || c > REQ_MAX_DN_LENGHT) {
                    p->state = REQ_ERROR;
                    return p->state;
                }
                p->dnlen = c;
                p->buf_idx = 0;
                p->state = REQ_DST_ADDR;
                break;

            case REQ_DST_ADDR:
                if (p->atyp == IPV4) {
                    p->buf[p->buf_idx++] = c;
                    if (p->buf_idx == 4) {
                        memcpy(&p->dst_addr.ipv4.s_addr, p->buf, 4);
                        p->buf_idx = 0;
                        p->state = REQ_DST_PORT;
                    }
                } else if (p->atyp == DOMAINNAME) {
                    p->dst_addr.domainname[p->buf_idx] = c;
                    p->buf_idx++;

                    if (p->buf_idx == p->dnlen) {
                        p->dst_addr.domainname[p->dnlen] = '\0';
                        p->buf_idx = 0;
                        p->state = REQ_DST_PORT;
                    }
                } else if (p->atyp == IPV6) {
                    p->buf[p->buf_idx++] = c;
                    if (p->buf_idx == 16) {
                        memcpy(&p->dst_addr.ipv6.s6_addr, p->buf, 16);
                        p->buf_idx = 0;
                        p->state = REQ_DST_PORT;
                    }
                }
                break;

            case REQ_DST_PORT:
                p->buf[p->buf_idx++] = c;
                if (p->buf_idx == 2) {
                    p->dst_port = (p->buf[0] << 8) | p->buf[1];
                    p->buf_idx = 0;
                    p->state = REQ_END;
                    return p->state;
                }
                break;

            case REQ_ERROR:
            case REQ_END:
                return p->state;
        }
    }
    return p->state;
}

bool has_request_read_ended(ReqParser *p) {
    return p != NULL && p->state == REQ_END;
}

bool has_request_errors(ReqParser *p) {
    return p == NULL || p->state == REQ_ERROR;
}

ReqCodes fill_request_answer(ReqParser *p, buffer *buffer, struct selector_key * key) {
    if (!buffer_can_write(buffer))
        return REQ_FULLBUFFER;

    SocksClient *data = ATTACHMENT(key);

    LOG_DEBUG("Filling request answer...\n");

    buffer_write(buffer, 0x05);             // VER
    buffer_write(buffer, p->status);        // REP
    buffer_write(buffer, 0x00);             // RSV

    // Ahora elegimos qué tipo de dirección mandamos
    ReqAtyp response_type = p->atyp;

    if (response_type == DOMAINNAME) {
        if (data->origin_resolution->ai_family == AF_INET)
            response_type = IPV4;
        else response_type = IPV6;
    }

    buffer_write(buffer, response_type);

    switch (response_type) {
        case IPV4:
            buffer_write(buffer, 127);
            buffer_write(buffer, 0);
            buffer_write(buffer, 0);
            buffer_write(buffer, 1);
            break;

        case IPV6: {
            uint8_t dummy_ipv6[16] = {0};
            for (int i = 0; i < 16; i++) {
                buffer_write(buffer, dummy_ipv6[i]);
            }
            break;
        }

        default:
            LOG_ERROR("Unsupported ATYP %d in fill_request_answer", response_type);
            return REQ_ERROR_GENERAL_FAILURE;
    }

    buffer_write(buffer, 0x00);
    buffer_write(buffer, 0x00);

    return REQ_OK;
}

const char* request_to_string(const ReqParser * p) {
    static char aux[REQ_MAX_DN_LENGHT + 64];
    static char to_string[REQ_MAX_DN_LENGHT];

    const char *prefix;

    switch (p->cmd) {
        case CONNECT: prefix = "Command CONNECT to:"; break;
        case BIND: prefix = "Command BIND to:"; break;
        case UDP_ASSOCIATE: prefix = "Command UDP_ASSOCIATE to:"; break;
        default: return "unknown unknown";
    }

    switch (p->atyp) {
        case IPV4:
            if (inet_ntop(AF_INET, &p->dst_addr.ipv4, to_string, sizeof(to_string)) == NULL)
                strncpy(to_string, "unknown4", sizeof(to_string));
            break;
        case IPV6:
            if (inet_ntop(AF_INET6, &p->dst_addr.ipv6, to_string, sizeof(to_string)) == NULL)
                strncpy(to_string, "unknown6", sizeof(to_string));
            break;
        case DOMAINNAME:
            strncpy(to_string, (char*)p->dst_addr.domainname + 1, p->dst_addr.domainname[0]);
            to_string[p->dst_addr.domainname[0]] = '\0';
            break;
        default:
            return "unknown unknown";
    }

    snprintf(aux, sizeof(aux), "%s %s %u", prefix, to_string, p->dst_port);
    return aux;
}
