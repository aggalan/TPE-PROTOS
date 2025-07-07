#ifndef AUTHENTICATION_PARSER_H
#define AUTHENTICATION_PARSER_H

#include "../buffer/buffer.h"
#include <stdint.h>
#include <stdbool.h>

#define USER_MAX_LEN  15
#define PASSWORD_MAX_LEN  15

typedef enum authentication_check {
    AUTH_SUCCESS = 0,
    AUTH_FAILURE
} AuthCheck;

typedef enum authentication_state {
    AUTH_VERSION = 0,
    USER_LENGTH,
    USERNAME,
    PASS_LENGTH,
    PASSWORD,
    AUTH_END,
    AUTH_ERROR
} AuthState;

typedef struct authentication_parser {
    uint8_t    version;
    uint8_t    ulen, plen;
    char       uname[USER_MAX_LEN + 1];
    char       passwd[PASSWORD_MAX_LEN + 1];
    uint8_t    idx;
    AuthCheck  auth_check;
    AuthState  state;
} AuthParser;

typedef enum authentication_status {
    AUTH_REPLY_OK = 0,
    AUTH_REPLY_DENIED,
    AUTH_REPLY_FULL_BUFFER,
    AUTH_REPLY_BAD_VERSION
} AuthCodes;

// Interface pública
void       init_authentication_parser(AuthParser *p);
AuthState  authentication_parse(AuthParser *p, buffer *b);
bool       has_authentication_read_ended(AuthParser *p);
bool       has_authentication_errors(AuthParser *p);
AuthCodes  fill_authentication_answer(AuthParser *p, buffer *b);

#endif // AUTHENTICATION_PARSER_H
