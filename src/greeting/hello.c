#include "hello.h"
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

void initNegotiationParser(NegParser * p) {
    if(p == NULL) {
        return;
    }
    p->state = VERSION;
    p->auth_method = NO_METHOD;
}

NegState negotiationParse(NegParser * p, buffer * buffer){
    printf("started parsing \n");
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
                printf("version %d\n", c);
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
                printf("number of methods %d\n", c);
                break;
            case METHODS:
                if(p->nmethods > 0) {
                    if ( c == NO_AUTH ){
                        p->auth_method = NO_AUTH;
                    } else if (c == USER_PASS) {
                        p->auth_method = USER_PASS;
                        p->state = END;
                    }
                    printf("method %d\n", c);
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
bool hasNegotiationReadEnded(NegParser * p){
    if(p == NULL) {
        return false;
    }
    return p->state == END;
}
bool hasNegotiationErrors(NegParser * p){
    if(p == NULL) {
        return false;
    }
    return p->state == FAIL;
}
NegCodes fillNegotiationAnswer(NegParser * p, buffer * buffer){
    if (!buffer_can_write(buffer))
        return FULLBUFFER;
    buffer_write(buffer, SOCKS_VERSION);
    buffer_write(buffer, p->auth_method);
    return OK;
}
