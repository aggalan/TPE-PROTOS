#include "authentication_parser.h"
#include <string.h>
#include <stdio.h>

#define CRED_FILE  "users.txt"


static bool
verify_credentials(const char *user, const char *pass) {
    FILE *f = fopen(CRED_FILE, "r");
    if (f == NULL)
        return false;

    char fuser[U_MAX_LEN + 1];
    char fpass[P_MAX_LEN + 1];

    while (fscanf(f, " %15s %15s", fuser, fpass) == 2) {
        if (strcmp(user, fuser) == 0 && strcmp(pass, fpass) == 0) {
            fclose(f);
            return true;
        }
    }
    fclose(f);
    return false;
}


void
init_authentication_parser(AuthParser *p) {
    if (p == NULL) return;
    memset(p, 0, sizeof(*p));
    p->state       = AUTH_VERSION;
    p->auth_check  = AUTH_DENIED;
}

AuthState
authentication_parse(AuthParser *p, buffer *b) {
    if (p == NULL || b == NULL) return AUTH_ERROR;

    while (buffer_can_read(b)) {
        uint8_t c = buffer_read(b);

        switch (p->state) {

            case AUTH_VERSION:
                if (c == VERSION) {
                    p->version = c;
                    p->state   = ULEN;
                } else {
                    p->state   = AUTH_ERROR;
                }
                break;

            case ULEN:
                if (c == 0 || c > U_MAX_LEN) {
                    p->state = AUTH_ERROR;
                    break;
                }
                p->ulen  = c;
                p->idx   = 0;
                p->state = UNAME;
                break;

            case UNAME:
                p->uname[p->idx++] = (char)c;
                if (p->idx == p->ulen) {
                    p->uname[p->idx] = '\0';
                    p->state = PLEN;
                }
                break;

            case PLEN:
                if (c == 0 || c > P_MAX_LEN) {
                    p->state = AUTH_ERROR;
                    break;
                }
                p->plen  = c;
                p->idx   = 0;
                p->state = PASSWD;
                break;

            case PASSWD:
                p->passwd[p->idx++] = (char)c;
                if (p->idx == p->plen) {
                    p->passwd[p->idx] = '\0';

                    p->auth_check = verify_credentials(p->uname, p->passwd)
                                    ? AUTH_SUCCESS : AUTH_DENIED;
                    p->state = AUTH_END;
                    return p->state;
                }
                break;

            case AUTH_END:
            case AUTH_ERROR:
                return p->state;
        }
    }
    return p->state;
}

bool
has_authentication_read_ended(AuthParser *p) {
    return p != NULL && p->state == AUTH_END;
}

bool
has_authentication_errors(AuthParser *p) {
    return p != NULL && p->state == AUTH_ERROR;
}

AuthCodes
fill_authentication_answer(AuthParser *p, buffer *b) {
    if (p == NULL || b == NULL)          return AUTH_ERROR;
    if (p->state != AUTH_END)            return AUTH_ERROR;

    buffer_write(b, VERSION);
    buffer_write(b, (p->auth_check == AUTH_SUCCESS) ? 0x00 : 0x01);
    return (p->auth_check == AUTH_SUCCESS) ? AUTH_OK : AUTH_DENIED;
}
