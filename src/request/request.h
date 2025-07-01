#ifndef REQUEST_H
#define REQUEST_H

#include <stdint.h>
#include "../buffer.h"
#include "../selector.h"
#include "request_parser.h"
#include <sys/socket.h>
#include <errno.h>

void request_init (const unsigned state, struct selector_key *key);
unsigned request_read (struct selector_key *key);
unsigned request_write (struct selector_key *key);


#endif //REQUEST_H
