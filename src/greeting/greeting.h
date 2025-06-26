#ifndef TPE_PROTOS_GREETING_H
#define TPE_PROTOS_GREETING_H

#include <stdint.h>
#include "buffer.h"
#include "selector.h"

struct hello_st {
    buffer               *rb;
    buffer               *wb;
    struct hello_parser   parser;
    uint8_t              method;
};

/* entry points for the state-machine */
void greeting_init (const unsigned state, struct selector_key *key);
unsigned greeting_read  (struct selector_key *key);
unsigned greeting_write (struct selector_key *key);
void greeting_close (const unsigned state, struct selector_key *key);

#endif /* TPE_PROTOS_GREETING_H */
