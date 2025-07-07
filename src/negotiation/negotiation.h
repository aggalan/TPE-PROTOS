#ifndef TPE_PROTOS_GREETING_H
#define TPE_PROTOS_GREETING_H

#include <stdint.h>
#include "../buffer/buffer.h"
#include "../selector/selector.h"
#include "negotiation_parser.h"

void negotiation_init (const unsigned state, struct selector_key *key);
unsigned negotiation_read  (struct selector_key *key);
unsigned negotiation_write (struct selector_key *key);
#endif /* TPE_PROTOS_GREETING_H */
