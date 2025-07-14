#include "request_parser.h"
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "../socks5/socks5.h"
#include "../logging/logger.h"

typedef ReqState (*parse_character)(ReqParser * parser, uint8_t byte);

static ReqState parse_version(ReqParser* p, uint8_t byte);
static ReqState parse_cmd(ReqParser* p, uint8_t byte);
static ReqState parse_rsv(ReqParser* p, uint8_t byte);
static ReqState parse_atyp(ReqParser* p, uint8_t byte);
static ReqState parse_dnlen(ReqParser* p, uint8_t byte);
static ReqState parse_dst_addr(ReqParser* p, uint8_t byte);
static ReqState parse_dst_port(ReqParser* p, uint8_t byte);
static ReqState parse_end(ReqParser* p, uint8_t byte);

static parse_character parse_functions[] = {
    [REQ_VERSION] = (parse_character) parse_version,
    [REQ_CMD] = (parse_character) parse_cmd,
    [REQ_RSV] = (parse_character) parse_rsv,
    [REQ_ATYP] = (parse_character) parse_atyp,
    [REQ_DNLEN] = (parse_character) parse_dnlen,
    [REQ_DST_ADDR] = (parse_character) parse_dst_addr,
    [REQ_DST_PORT] = (parse_character) parse_dst_port,
    [REQ_ERROR] = (parse_character) parse_end,
    [REQ_END] = (parse_character) parse_end,
};

void init_request_parser(ReqParser *parser) {
    if (parser == NULL) return;
    memset(parser, 0, sizeof(ReqParser));
    parser->state = REQ_VERSION;
    parser->status = REQ_SUCCEDED;
    parser->buf_idx = 0;
}

ReqState request_parse(ReqParser* parser, buffer* buffer) {
    LOG_DEBUG("request_parse: Starting request parse...\n");
    if (parser == NULL || buffer == NULL) return REQ_ERROR;
    while (buffer_can_read(buffer)) {
        parser->state=parse_functions[parser->state](parser,buffer_read(buffer));
        if (parser->state == REQ_END || parser->state == REQ_ERROR) {
            break;
        }
    }
    return parser->state==REQ_END ? parse_end(parser,0):parser->state;
}

ReqState parse_version(ReqParser * parser, uint8_t byte) {
    LOG_DEBUG("parse_version: Parsing byte %d\n", byte);
    if (byte == SOCKS_VERSION) {
        LOG_DEBUG("parse_version: SOCKS version %d accepted\n", byte);
        parser->version = byte;
        return REQ_CMD;
    }
    LOG_DEBUG("parse_version: Invalid SOCKS version %d\n", byte);
    return REQ_ERROR;
}

ReqState parse_cmd(ReqParser * parser, uint8_t byte) {
    LOG_DEBUG("parse_cmd: Parsing command byte %d\n", byte);
    if (byte == CONNECT) {
        parser->cmd = byte;
        return REQ_RSV;
    }
    LOG_DEBUG("parse_cmd: Unsupported command %d\n", byte);
    parser->status = REQ_ERROR_COMMAND_NOT_SUPPORTED;
    return REQ_ERROR;
}

ReqState parse_rsv(ReqParser * parser, uint8_t byte) {
    if(parser->state!= REQ_RSV) {
        LOG_DEBUG("parse_rsv: Invalid state %d for reserved byte %d\n", parser->state, byte);
        return REQ_ERROR;
    }
    LOG_DEBUG("parse_rsv: Parsing reserved byte %d\n", byte);
    if (byte == 0x00) {
        return REQ_ATYP;
    }
    LOG_DEBUG("parse_rsv: Invalid reserved byte %d\n", byte);
    return REQ_ERROR;
}

ReqState parse_atyp(ReqParser * parser, uint8_t byte) {
    LOG_DEBUG("parse_atyp: Parsing address type byte %d\n", byte);
    switch (byte) {
        case IPV4:
            parser->atyp = IPV4;
            return REQ_DST_ADDR;
        case IPV6:
            parser->atyp = byte;
            return REQ_DST_ADDR;
        case DOMAINNAME:
            parser->atyp = byte;
            return REQ_DNLEN;
        default:
            LOG_DEBUG("parse_atyp: Unsupported address type %d\n", byte);
            return REQ_ERROR;
    }
}

ReqState parse_dnlen(ReqParser * parser, uint8_t byte) {
    LOG_DEBUG("parse_dnlen: Parsing domain name length byte %d\n", byte);
    if (byte == 0) {
        LOG_DEBUG("parse_dnlen: Domain name length %d invalid\n", byte);
        return REQ_ERROR;
    }
    parser->dnlen = byte;
    parser->buf_idx = 0;
    return REQ_DST_ADDR;
}

ReqState parse_dst_addr(ReqParser * parser, uint8_t byte) {
    LOG_DEBUG("parse_dst_addr: Parsing destination address byte %d\n", byte);
    switch (parser->atyp) {
        case IPV4:
            parser->buf[parser->buf_idx++] = byte;
            if (parser->buf_idx == 4) {
                memcpy(&parser->dst_addr.ipv4.s_addr, parser->buf, 4);
                parser->buf_idx = 0;
                return REQ_DST_PORT;
            }
            break;
        case IPV6:
            parser->buf[parser->buf_idx++] = byte;
            if (parser->buf_idx == 16) {
                memcpy(&parser->dst_addr.ipv6.s6_addr, parser->buf, 16);
                parser->buf_idx = 0;
                return REQ_DST_PORT;
            }
            break;
        case DOMAINNAME:
            parser->dst_addr.domainname[parser->buf_idx++] = byte;
            if (parser->buf_idx == parser->dnlen) {
                parser->dst_addr.domainname[parser->dnlen] = '\0'; // Null-terminate
                parser->buf_idx = 0;
                return REQ_DST_PORT;
            }
            break;
        default:
            LOG_DEBUG("parse_dst_addr: Unsupported ATYP %d\n", parser->atyp);
            return REQ_ERROR;
    }
    return REQ_DST_ADDR;
}

ReqState parse_dst_port(ReqParser * parser, uint8_t byte) {
    LOG_DEBUG("parse_dst_port: Parsing destination port byte %d\n", byte);
    parser->buf[parser->buf_idx++] = byte;
    if (parser->buf_idx == 2) {
        parser->dst_port = (parser->buf[0] << 8) | parser->buf[1];
        parser->buf_idx = 0;
        return REQ_END;
    }
    return REQ_DST_PORT;
}

ReqState parse_end(ReqParser * parser, uint8_t byte) {
    return (parser != NULL && parser->state == REQ_END && byte==0) ? REQ_END : REQ_ERROR;
}

bool has_request_read_ended(ReqParser *parser) {
    return parser->state == REQ_END || parser->state == REQ_ERROR;
}

bool has_request_errors(ReqParser *parser) {
    return parser == NULL || parser->state == REQ_ERROR;
}

ReqCodes fill_request_answer(ReqParser *parser, buffer *buffer, struct selector_key * key) {
    if (!buffer_can_write(buffer))
        return REQ_FULLBUFFER;

    SocksClient *data = ATTACHMENT(key);

    LOG_DEBUG("Filling request answer...\n");

    buffer_write(buffer, 0x05);             // VER
    buffer_write(buffer, parser->status);        // REP
    buffer_write(buffer, 0x00);             // RSV

    // Ahora elegimos qué tipo de dirección mandamos
    ReqAtyp response_type = parser->atyp;

    if (response_type == DOMAINNAME && parser->status == REQ_SUCCEDED ) {
        if (data->origin_resolution->ai_family == AF_INET)
            response_type = IPV4;
        else response_type = IPV6;
    }

    if (parser->status != REQ_SUCCEDED) {
        response_type = IPV4;
    }


    buffer_write(buffer, response_type);

    switch (response_type) {
        case IPV4:
            buffer_write(buffer, 0);
            buffer_write(buffer, 0);
            buffer_write(buffer, 0);
            buffer_write(buffer, 0);
            break;

        case IPV6: {
            uint8_t dummy_ipv6[16] = {0};
            for (int i = 0; i < 16; i++) {
                buffer_write(buffer, dummy_ipv6[i]);
            }
            break;
        }

        default:
            LOG_DEBUG("Unsupported ATYP %d in fill_request_answer", response_type);
            return REQ_UNSUPORTED_ATYP;
    }

    buffer_write(buffer, 0x00);
    buffer_write(buffer, 0x00);

    return REQ_OK;
}

const char* request_to_string(const ReqParser * parser) {
    static char aux[REQ_MAX_DN_LENGHT + 64];
    static char to_string[REQ_MAX_DN_LENGHT];

    const char *prefix;

    switch (parser->cmd) {
        case CONNECT: prefix = "Command CONNECT to:"; break;
        case BIND: prefix = "Command BIND to:"; break;
        case UDP_ASSOCIATE: prefix = "Command UDP_ASSOCIATE to:"; break;
        default: return "unknown unknown";
    }

    switch (parser->atyp) {
        case IPV4:
            if (inet_ntop(AF_INET, &parser->dst_addr.ipv4, to_string, sizeof(to_string)) == NULL)
                strncpy(to_string, "unknown4", sizeof(to_string));
            break;
        case IPV6:
            if (inet_ntop(AF_INET6, &parser->dst_addr.ipv6, to_string, sizeof(to_string)) == NULL)
                strncpy(to_string, "unknown6", sizeof(to_string));
            break;
        case DOMAINNAME:
            strncpy(to_string, (char*)parser->dst_addr.domainname + 1, parser->dst_addr.domainname[0]);
            to_string[parser->dst_addr.domainname[0]] = '\0';
            break;
        default:
            return "unknown unknown";
    }

    snprintf(aux, sizeof(aux), "%s %s %u", prefix, to_string, parser->dst_port);
    return aux;
}
