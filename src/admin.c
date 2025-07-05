// admin.c
#include "admin.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define USER_FILE "./src/authentication/users.txt"

int admin_add_user(const char *username, const char *password) {
    if (!username || !password) return -1;
    FILE *f = fopen(USER_FILE, "a");
    if (!f) return -1;
    fprintf(f, "%s %s\n", username, password);
    fclose(f);
    return 0;
}

int admin_del_user(const char *username) {
    if (!username) return -1;

    FILE *in = fopen(USER_FILE, "r");
    if (!in) return -1;
    FILE *out = fopen(USER_FILE ".tmp", "w");
    if (!out) {
        fclose(in);
        return -1;
    }

    char u[256], p[256];
    int removed = 0;
    while (fscanf(in, "%255s %255s", u, p) == 2) {
        if (strcmp(u, username) != 0) {
            fprintf(out, "%s %s\n", u, p);
        } else {
            removed = 1;
        }
    }

    fclose(in);
    fclose(out);

    if (rename(USER_FILE ".tmp", USER_FILE) != 0) {
        remove(USER_FILE ".tmp");
        return -1;
    }

    return removed ? 0 : -1;
}

char* admin_list_users(void) {
    FILE *f = fopen(USER_FILE, "r");
    if (!f) return NULL;

    size_t cap = 1024;
    char *buf = malloc(cap);
    if (!buf) { fclose(f); return NULL; }
    buf[0] = '\0';
    size_t len = 0;

    char user[256], pass[256];
    while (fscanf(f, "%255s %255s", user, pass) == 2) {
        size_t needed = strlen(user) + 2;
        if (len + needed > cap) {
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) { free(buf); fclose(f); return NULL; }
            buf = tmp;
        }
        strcat(buf, user);
        strcat(buf, "\n");
        len += needed - 1;
    }
    fclose(f);
    return buf;
}
