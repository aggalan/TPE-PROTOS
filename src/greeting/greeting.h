#ifndef TPE_PROTOS_GREETING_H
#define TPE_PROTOS_GREETING_H

#include <stdint.h>
#include "../buffer.h"
#include "../selector.h"
#include "hello.h"

struct hello_st {
    struct hello_parser   parser;
    uint8_t              method;
};

void greeting_init (const unsigned state, struct selector_key *key);
unsigned greeting_read  (struct selector_key *key);
unsigned greeting_write (struct selector_key *key);

#endif /* TPE_PROTOS_GREETING_H */
