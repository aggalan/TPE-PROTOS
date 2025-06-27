#include "request_parser.h"
#include <string.h>
#include <stdio.h>

void init_request_parser(ReqParser *p) {
    if (p == NULL) return;
    memset(p, 0, sizeof(ReqParser));
    p->state = REQ_VERSION;
    p->status = REQ_SUCCEDED;
    p->buf_idx = 0;
}

ReqState request_parse(ReqParser* p, buffer* b) {
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

ReqCodes
fill_request_answer(ReqParser *p, buffer *b) {

    if (!buffer_can_write(b))
        return REQ_FULLBUFFER;

    printf("Filling request answer... \n");

    uint8_t rep = 0x01;
    switch (p->status) {
        case REQ_SUCCEDED:                       rep = 0x00; break;
        case REQ_ERROR_GENERAL_FAILURE:          rep = 0x01; break;
        case REQ_ERROR_CONNECTION_NOT_ALLOWED:   rep = 0x02; break;
        case REQ_ERROR_NTW_UNREACHABLE:          rep = 0x03; break;
        case REQ_ERROR_HOST_UNREACHABLE:         rep = 0x04; break;
        case REQ_ERROR_CONNECTION_REFUSED:       rep = 0x05; break;
        case REQ_ERROR_TTL_EXPIRED:              rep = 0x06; break;
        case REQ_ERROR_COMMAND_NOT_SUPPORTED:    rep = 0x07; break;
        case REQ_ERROR_ADDRESS_TYPE_NOT_SUPPORTED: rep = 0x08; break;
    }

    buffer_write(b, SOCKS_VERSION);
    buffer_write(b, rep);
    buffer_write(b, 0x00);
    buffer_write(b, IPV4);

    buffer_write(b, 0x00);
    buffer_write(b, 0x00);
    buffer_write(b, 0x00);
    buffer_write(b, 0x00);

    buffer_write(b, 0x00);
    buffer_write(b, 0x00);
//    ESTO ES TEMPORAL
    return REQ_OK;
}

