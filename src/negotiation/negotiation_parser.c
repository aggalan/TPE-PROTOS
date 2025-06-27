#include "negotiation_parser.h"
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

void init_negotiation_parser(NegParser * p) {
    if(p == NULL) {
        return;
    }
    p->state = NEG_VERSION;
    p->auth_method = NO_METHOD;
}

NegState negotiation_parse(NegParser * p, buffer * buffer){
    printf("Started parsing negotiation... \n");
    if(p == NULL || buffer == NULL) {
        return NEG_ERROR;
    }
    while(buffer_can_read(buffer)) {
        uint8_t c = buffer_read(buffer);
        switch(p->state) {
            case NEG_VERSION:
                if(c == SOCKS_VERSION) {
                    p->version = c;
                    p->state = NEG_NMETHODS;
                } else {
                    p->state = NEG_ERROR;
                }
                printf("VERSION: %d\n", c);
                break;
            case NEG_NMETHODS:
                if(c > 0 && c <= METHOD_SIZE) {
                    p->nmethods = c;
                    p->state = NEG_METHODS;
                } else if (c == 0) {
                    p->state = NEG_END;
                } else {
                    p->state = NEG_ERROR;
                }
                printf("NMETHODS: %d\n", c);
                break;
            case NEG_METHODS:
                if (c == USER_PASS) {
                    p->auth_method = USER_PASS;
                } else if (c == NO_AUTH) {
                    if (p->auth_method != USER_PASS)
                        p->auth_method = NO_AUTH;
                }
                if (--p->nmethods == 0)
                    p->state = NEG_END;
                break;
            case NEG_END:
                return NEG_END;
            case NEG_ERROR:
                return NEG_ERROR;

        }
        if(p->state == NEG_ERROR) {
            return NEG_ERROR;
        }
    }

    return p->state;
}
bool has_negotiation_read_ended(NegParser * p){
    if(p == NULL) {
        return false;
    }
    return p->state == NEG_END;
}
bool has_negotiation_errors(NegParser * p){
    if(p == NULL) {
        return false;
    }
    return p->state == NEG_ERROR;
}
NegCodes fill_negotiation_answer(NegParser * p, buffer * buffer){
    if (!buffer_can_write(buffer))
        return NEG_FULL_BUFFER;
    printf("Filling negotiation answer... \n");
    buffer_write(buffer, SOCKS_VERSION);
    buffer_write(buffer, p->auth_method);
    if (p->auth_method == NO_METHOD)
        return NEG_INVALID_METHOD;
    return NEG_OK;
}
