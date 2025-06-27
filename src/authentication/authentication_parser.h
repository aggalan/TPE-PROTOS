#ifndef AUTHENTICATION_PARSER_H
#define AUTHENTICATION_PARSER_H

#define U_MAX_LEN 15
#define P_MAX_LEN 15
#include "../buffer.h"
#include <stdint.h>
#define SOCKS_VERSION 0x05


typedef enum authentication_check {
    AUTH_SUCCESS = 0,
    AUTH_DENIED,
} AuthCheck;


typedef enum authentication_state {
    AUTH_VERSION = 0,
    ULEN,
    UNAME,
    PLEN,
    PASSWD,
    AUTH_END,
    AUTH_ERROR
} AuthState;

typedef struct authentication_parser {
    uint8_t version;
    uint8_t ulen;
    uint8_t plen;
    AuthCheck auth_check;
    AuthState state;
} AuthParser;

typedef enum authentication_status {
    AUTH_OK = 0,
    AUTH_FULL_BUFFER,
    AUTH_INVALID_VERSION,
} AuthCodes;


void init_authentication_parser(AuthParser * p);
AuthState authentication_parse(AuthParser * p, buffer * buffer);
bool has_authentication_read_ended(AuthParser * p);
bool has_authentication_errors(AuthParser * p);
AuthCodes fill_authentication_answer(AuthParser * p,buffer * buffer);

#endif
