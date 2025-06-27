#ifndef AUTHENTICATION_PARSER_H
#define AUTHENTICATION_PARSER_H

#include "../buffer.h"
#include <stdint.h>
#include <stdbool.h>

#define VERSION    0x01
#define U_MAX_LEN  15
#define P_MAX_LEN  15

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
    uint8_t  version;
    uint8_t  ulen, plen;
    char     uname[U_MAX_LEN + 1];
    char     passwd[P_MAX_LEN + 1];
    uint8_t  idx;
    AuthCheck  auth_check;
    AuthState  state;
} AuthParser;

typedef enum authentication_status {
    AUTH_OK = 0,
    AUTH_FULL_BUFFER,
    AUTH_INVALID_VERSION,
} AuthCodes;

void       init_authentication_parser(AuthParser *p);
AuthState  authentication_parse(AuthParser *p, buffer *b);
bool       has_authentication_read_ended(AuthParser *p);
bool       has_authentication_errors(AuthParser *p);
AuthCodes  fill_authentication_answer(AuthParser *p, buffer *b);

#endif
