#include "request_parser.h"
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "../logging/logger.h"

void init_request_parser(ReqParser *p) {
    if (p == NULL) return;
    memset(p, 0, sizeof(ReqParser));
    p->state = REQ_VERSION;
    p->status = REQ_SUCCEDED;
    p->buf_idx = 0;
}

ReqState request_parse(ReqParser* p, buffer* b) {
    LOG_INFO("Starting request parse...\n");
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
                    p->dst_addr.domainname[p->buf_idx + 1] = c;
                    p->buf_idx++;
                    if (p->buf_idx == p->dnlen) {
                        p->dst_addr.domainname[0] = p->dnlen;
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

ReqCodes fill_request_answer(ReqParser *p, buffer *buffer) {
    if (!buffer_can_write(buffer))
        return REQ_FULLBUFFER;

    uint8_t answer[10] = {
        0x05,
        p->status,
        0x00,
        p->atyp,
        127, 0, 0, 1,
        0x30, 0x39
    };


    LOG_INFO("Filling request answer...\n");


    for (int i = 0; i < 10; i++) {
        if (!buffer_can_write(buffer)) {
            return REQ_FULLBUFFER;
        }
        buffer_write(buffer, answer[i]);
    }

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
