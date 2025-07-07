#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H


#include "../selector/selector.h"

void authentication_init(const unsigned state,struct selector_key *key);
unsigned authentication_read(struct selector_key *key);
unsigned authentication_write(struct selector_key *key);

#endif //AUTHENTICATION_H
