#include "authentication_parser.h"
#include <stddef.h>
#include <stdio.h>

#include "negotiation_parser.h"

void init_authentication_parser(AuthParser * p) {
    if(p == NULL) {
        return;
    }
    p->state = AUTH_VERSION;
    p->auth_check = AUTH_DENIED;
}
AuthState authentication_parse(AuthParser * p, buffer * buffer) {
    printf("Started parsing authentication... \n");
    if(p == NULL || buffer == NULL) {
        return AUTH_ERROR;
    }
    while(buffer_can_read(buffer)) {
        uint8_t c = buffer_read(buffer);
        switch(p->state) {
            case AUTH_VERSION:
                if(c == VERSION) {
                    p->version = c;
                    p->state = ULEN;
                } else {
                    p->state = AUTH_ERROR;
                }
                printf("VERSION: %d\n", c);
                break;
            case ULEN:
                if(c > 0 && c <= U_MAX_LEN) {
                    p->ulen = c;
                    p->state = UNAME;
                }  else {
                    p->state = NEG_ERROR;
                }
                printf("ULEN: %d\n", c);
                break;
            case UNAME:
                if(c > 0 && c <= p->ulen) {
                    printf("UNAME: %c\n", c);
                    p->state = PLEN;
                } else {
                    p->state = NEG_ERROR;
                }
                break;
            case PLEN:
                if(c > 0 && c <= P_MAX_LEN) {
                    p->ulen = c;
                    p->state = UNAME;
                }  else {
                    p->state = NEG_ERROR;
                }
                printf("PLEN: %d\n", c);
                break;
            case PASSWD:
                if(c > 0 && c <= p->plen) {
                    printf("PASSWD: %c\n", c);
                    p->auth_check = AUTH_SUCCESS;
                    p->state = AUTH_END;
                } else {
                    p->state = NEG_ERROR;
                }
                break;
            case AUTH_END:
                return AUTH_END;
            case AUTH_ERROR:
                return AUTH_ERROR;

        }
        if(p->state == NEG_ERROR) {
            return NEG_ERROR;
        }
    }
}
bool has_authentication_read_ended(AuthParser * p){
    if(p == NULL) {
        return false;
    }
    return p->state == AUTH_END;
}
bool has_authentication_errors(AuthParser * p){
    if(p == NULL) {
        return false;
    }
    return p->state == AUTH_ERROR;
}
AuthCodes fill_authentication_answer(AuthParser * p,buffer * buffer){
    if(p == NULL || buffer == NULL) {
        return AUTH_ERROR;
    }
    if(p->state != AUTH_END) {
        return AUTH_ERROR;
    }
    if(p->auth_check == AUTH_SUCCESS) {
        buffer_write(buffer, VERSION);
        buffer_write(buffer, 0x00);
        return AUTH_OK;
    } else {
        buffer_write(buffer, VERSION);
        buffer_write(buffer, 0x01);
        return AUTH_DENIED;
    }
}