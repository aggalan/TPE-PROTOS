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

typedef struct hello_parser {
    uint8_t version;
    uint8_t nmethods;
    AcceptedMethods auth_method;
    uint8_t selected;
    NegState state;
} NegParser;

typedef enum negotiation_status {
    OK = 0,
    FULLBUFFER,
    INVALIDMETHOD,
} NegCodes;


void initNegotiationParser(struct hello_parser * p);
NegState negotiationParse(struct hello_parser * p, buffer * buffer);
bool hasNegotiationReadEnded(struct hello_parser * p);
bool hasNegotiationErrors(struct hello_parser * p);
NegCodes fillNegotiationAnswer(struct hello_parser * p,buffer * buffer);




#endif //TPE_PROTOS_HELLO_H
