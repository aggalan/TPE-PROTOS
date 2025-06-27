#ifndef REQUEST_PARSER_H
#define REQUEST_PARSER_H
#include "../buffer.h"
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define METHOD_SIZE 255
#define SOCKS_VERSION 0x05
#define PORT_BYTE_LENGHT 2
#define REQ_MAX_DN_LENGHT 0xFF

typedef enum cmd {
    CONNECT = 0x01,
    BIND = 0x02,
    UDP_ASSOCIATE = 0x03
} ReqCmd;

typedef enum atyp {
    IPV4 = 0x01,
    DOMAINNAME = 0x03,
    IPV6 = 0x04
} ReqAtyp;


typedef union adress {
    struct in_addr ipv4;
    uint8_t domainname[REQ_MAX_DN_LENGHT + 1];
    struct in6_addr ipv6;


 //   uint8_t bytes[REQ_MAX_DN_LENGHT + 1];
} ReqAddr;

typedef enum request_state {
    REQ_VERSION = 0,
    REQ_CMD,
    REQ_RSV,
    REQ_ATYP,
    REQ_DNLEN,
    REQ_DST_ADDR,
    REQ_DST_PORT,
    REQ_ERROR,
    REQ_END
} ReqState;

typedef enum request_status {
    REQ_SUCCEDED = 0,
    REQ_ERROR_GENERAL_FAILURE,
    REQ_ERROR_CONNECTION_NOT_ALLOWED,
    REQ_ERROR_NTW_UNREACHABLE,
    REQ_ERROR_HOST_UNREACHABLE,
    REQ_ERROR_CONNECTION_REFUSED,
    REQ_ERROR_TTL_EXPIRED,
    REQ_ERROR_COMMAND_NOT_SUPPORTED,
    REQ_ERROR_ADDRESS_TYPE_NOT_SUPPORTED,
} ReqStatus;


typedef struct request_parser {
    uint8_t version;
    ReqCmd cmd;
    ReqAtyp atyp;
    uint8_t dnlen;
    ReqAddr dst_addr;
    in_port_t dst_port;
    ReqState state;
    ReqStatus status;
    uint8_t buf[16];   // buffer temporal reutilizable (16 bytes para IPv6, suficiente para dirección y puerto)
    uint8_t buf_idx;   // índice de progreso en el buffer
} ReqParser;

typedef enum request_codes {
    REQ_OK = 0,
    REQ_FULLBUFFER,
} ReqCodes;

//const char * reqParserToString(const ReqParser * p);

void init_request_parser(ReqParser * p);

ReqState request_parse(ReqParser* p, buffer* buffer);

bool has_request_read_ended(ReqParser * p);

bool has_request_errors(ReqParser * p);

ReqCodes fill_request_answer( ReqParser * p, buffer* buffer);

#endif //REQUEST_PARSER_H
