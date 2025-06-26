#include "negotiation_parser.h"
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

void init_negotiation_parser(NegParser * p) {
    if(p == NULL) {
        return;
    }
    p->state = VERSION;
    p->auth_method = NO_METHOD;
}

NegState negotiation_parse(NegParser * p, buffer * buffer){
    printf("Started parsing negotiation... \n");
    if(p == NULL || buffer == NULL) {
        return FAIL;
    }
    while(buffer_can_read(buffer)) {
        uint8_t c = buffer_read(buffer);
        switch(p->state) {
            case VERSION:
                if(c == SOCKS_VERSION) {
                    p->version = c;
                    p->state = NUMBER;
                } else {
                    p->state = FAIL;
                }
                printf("VERSION: %d\n", c);
                break;
            case NUMBER:
                if(c > 0 && c <= METHOD_SIZE) {
                    p->nmethods = c;
                    p->state = METHODS;
                } else if (c == 0) {
                    p->state = END;
                } else {
                    p->state = FAIL;
                }
                printf("NMETHODS: %d\n", c);
                break;
            case METHODS:
                if(p->nmethods > 0) {
                    if ( c == NO_AUTH ){
                        p->auth_method = NO_AUTH;
                        printf("NO_AUTH_METHOD \n");

                    } else if (c == USER_PASS) {
                        p->auth_method = USER_PASS;
                        printf("USER_PASS_METHOD \n");
                        p->state = END;
                    }
                } else {
                    p->state = FAIL;
                }
                break;
            case END:
                return END;
            case FAIL:
                return FAIL;

        }
        if(p->state == FAIL) {
            return FAIL;
        }
    }

    return p->state;
}
bool has_negotiation_read_ended(NegParser * p){
    if(p == NULL) {
        return false;
    }
    return p->state == END;
}
bool has_negotiation_errors(NegParser * p){
    if(p == NULL) {
        return false;
    }
    return p->state == FAIL;
}
NegCodes fill_negotiation_answer(NegParser * p, buffer * buffer){
    if (!buffer_can_write(buffer))
        return FULL_BUFFER;
    buffer_write(buffer, SOCKS_VERSION);
    buffer_write(buffer, p->auth_method);
    return OK;
}
