#include "admin.h"
#include <stdio.h>
#include <string.h>

#define USER_FILE "./src/authentication/users.txt"

int admin_add_user(const char *username, const char *password) {
    if(username == NULL || password == NULL) {
        return -1;
    }

    FILE *f = fopen(USER_FILE, "a");
    if(f == NULL) {
        return -1;
    }
    fprintf(f, "%s %s\n", username, password);
    fclose(f);
    return 0;
}
