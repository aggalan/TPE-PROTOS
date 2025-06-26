//
// Created by Agustin Galan on 26/06/2025.
//

#ifndef AUTHENTICATION_PARSER_H
#define AUTHENTICATION_PARSER_H

#include <stdint.h>
#include "../buffer.h"
#include "../selector.h"
#include "authentication_parser.h"

void authentication_init (const unsigned state, struct selector_key *key);
unsigned authentication_read  (struct selector_key *key);
unsigned authentication_write (struct selector_key *key);


#endif //AUTHENTICATION_PARSER_H
