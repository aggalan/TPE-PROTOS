#ifndef TPE_PROTOS_GREETING_H
#define TPE_PROTOS_GREETING_H

#include <stdint.h>
#include "../buffer.h"
#include "../selector.h"
#include "negotiation_parser.h"

struct hello_st {
    struct negotiation_parser   parser;
    uint8_t              method;
};

void negotiation_init (const unsigned state, struct selector_key *key);
unsigned negotiation_read  (struct selector_key *key);
unsigned negotiation_write (struct selector_key *key);

#endif /* TPE_PROTOS_GREETING_H */
