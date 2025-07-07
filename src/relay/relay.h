
#ifndef RELAY_H
#define RELAY_H

#include <stdbool.h>
#include <sys/types.h>
#include "../buffer/buffer.h"
#include "../socks5/socks5.h"
#include "../selector/selector.h"
#include "../stm/stm.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void relay_init(const unsigned state, struct selector_key *key);
unsigned relay_read(struct selector_key *key);
unsigned relay_write(struct selector_key *key);
void relay_close(struct selector_key *key);


#endif //RELAY_H
