//
// Created by Agustin Galan on 26/06/2025.
//


#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include "../selector.h"

void authentication_init(const unsigned state,struct selector_key *key);

unsigned authentication_read(struct selector_key *key);

#endif //AUTHENTICATION_H
