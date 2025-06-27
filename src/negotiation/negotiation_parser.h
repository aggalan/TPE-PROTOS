#ifndef TPE_PROTOS_HELLO_H
#define TPE_PROTOS_HELLO_H
#define METHOD_SIZE 255
#include "../buffer.h"
#include <stdint.h>

#define SOCKS_VERSION 0x05

typedef enum accepted_methods {
    NO_AUTH = 0x00,
    USER_PASS = 0x02,
    NO_METHOD = 0xFF
} AcceptedMethods;


typedef enum negotiation_state {
    NEG_VERSION = 0,
    NEG_NMETHODS,
    NEG_METHODS,
    NEG_END,
    NEG_ERROR
} NegState;

typedef struct negotiation_parser {
    uint8_t version;
    uint8_t nmethods;
    AcceptedMethods auth_method;
    NegState state;
} NegParser;

typedef enum negotiation_status {
    NEG_OK = 0,
    NEG_FULL_BUFFER,
    NEG_INVALID_METHOD,
} NegCodes;


void init_negotiation_parser(NegParser * p);
NegState negotiation_parse(NegParser * p, buffer * buffer);
bool has_negotiation_read_ended(NegParser * p);
bool has_negotiation_errors(NegParser * p);
NegCodes fill_negotiation_answer(NegParser * p,buffer * buffer);




#endif //TPE_PROTOS_HELLO_H
