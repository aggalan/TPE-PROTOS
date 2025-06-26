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
    VERSION = 0,
    NUMBER,
    METHODS,
    END,
    FAIL
} NegState;

typedef struct negotiation_parser {
    uint8_t version;
    uint8_t nmethods;
    AcceptedMethods auth_method;
    NegState state;
} NegParser;

typedef enum negotiation_status {
    OK = 0,
    FULL_BUFFER,
    INVALID_METHOD,
} NegCodes;


void init_negotiation_parser(struct negotiation_parser * p);
NegState negotiation_parse(struct negotiation_parser * p, buffer * buffer);
bool has_negotiation_read_ended(struct negotiation_parser * p);
bool has_negotiation_errors(struct negotiation_parser * p);
NegCodes fill_negotiation_answer(struct negotiation_parser * p,buffer * buffer);




#endif //TPE_PROTOS_HELLO_H
