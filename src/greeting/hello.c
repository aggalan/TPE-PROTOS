#include "hello.h"
#include <stdint.h>
#include <stddef.h>
#

#define VERSION_5 0x05

void initNegotiationParser(struct hello_parser * p) {
    if(p == NULL) {
        return;
    }
    p->state = VERSION;
    p->auth_method = NO_METHOD;
}

NegState negotiationParse(struct hello_parser * p, buffer * buffer){
    if(p == NULL || buffer == NULL) {
        return ERROR;
    }
    while(buffer_can_read(buffer)) {
        uint8_t c = buffer_read(buffer);
        switch(p->state) {
            case VERSION:
                if(c == VERSION_5) {
                    p->version = c;
                    p->state = NUMBER;
                } else {
                    p->state = ERROR;
                }
                break;
            case NUMBER:
                if(c > 0 && c <= METHOD_SIZE) {
                    p->nmethods = c;
                    p->state = METHODS;
                } else if (c == 0) {
                    p->state = END;
                } else {
                    p->state = ERROR;
                }
                break;
            case METHODS:
                if(p->nmethods > 0) {
                    if ( c == NO_AUTH ){
                        p->auth_method = NO_AUTH;
                    } else if (c == USER_PASS) {
                        p->auth_method = USER_PASS;
                        p->state = END;
                    }
                } else {
                    p->state = ERROR;
                }
                break;
            case END:
                return END;
            case ERROR:
                return ERROR;

        }
        if(p->state == ERROR) {
            return ERROR;
        }
    }
}
bool hasNegotiationReadEnded(struct hello_parser * p){
    if(p == NULL) {
        return false;
    }
    return p->state == END;
}
bool hasNegotiationErrors(struct hello_parser * p){
    if(p == NULL) {
        return false;
    }
    return p->state == ERROR;
}
NegCodes fillNegotiationAnswer(struct hello_parser * p, buffer * buffer){
    if (!buffer_can_write(buffer))
        return FULLBUFFER;
    buffer_write(buffer, VERSION_5);
    buffer_write(buffer, p->auth_method);
    return OK;
}
