#include "admin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LIMIT_USERS 10

static int ensure_file_exists(void) {
    FILE *f = fopen(USER_FILE, "a");
    if (!f) return -1;
    fclose(f);
    return 0;
}

int admin_add_user(const char *username, const char *password) {
    if (!username || !password) return -1;
    if (ensure_file_exists() != 0) return -1;
    FILE *f = fopen(USER_FILE, "r");
    if (!f) return -1;
    char line[512], u[256], p[256];
    int user_count = 0;
    while (fgets(line, sizeof line, f) && user_count < LIMIT_USERS) {
        if (sscanf(line, "%255s %255s", u, p) == 2) {
            if (strcmp(u, username) == 0) {
                fclose(f);
                return -1;
            }
            user_count++;
        }
    }
    fclose(f);

    f = fopen(USER_FILE, "a");
    if (!f) return -1;
    int ok = fprintf(f, "%s %s\n", username, password) > 0 ? 0 : -1;
    fclose(f);
    return ok;
}

int admin_del_user(const char *username) {
    if (!username) return -1;
    if (ensure_file_exists() != 0) return -1;

    FILE *in = fopen(USER_FILE, "r");
    if (!in) return -1;
    FILE *out = fopen(USER_FILE ".tmp", "w");
    if (!out) { fclose(in); return -1; }

    char line[512], u[256], p[256];
    int found = 0;
    while (fgets(line, sizeof line, in)) {
        if (sscanf(line, "%255s %255s", u, p) == 2 && strcmp(u, username) == 0) {
            found = 1;
            continue;
        }
        fputs(line, out);
    }
    fclose(in);
    fclose(out);

    if (!found) {
        remove(USER_FILE ".tmp");
        return -1;
    }
    if (rename(USER_FILE ".tmp", USER_FILE) != 0) return -1;
    return 0;
}

char *admin_list_users(void) {
    if (ensure_file_exists() != 0) return NULL;

    FILE *f = fopen(USER_FILE, "r");
    if (!f) return NULL;
    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    if (!out) { fclose(f); return NULL; }
    out[0] = '\0';

    char line[512], u[256], p[256];
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%255s %255s", u, p) == 2) {
            size_t need = strlen(u) + 2;
            if (len + need + 1 > cap) {
                cap = (len + need + 1) * 2;
                out = realloc(out, cap);
                if (!out) { fclose(f); return NULL; }
            }
            strcpy(out + len, u);
            len += strlen(u);
            out[len++] = '\n';
            out[len] = '\0';
        }
    }
    fclose(f);

    if (len > 0 && out[len-1] == '\n') out[len-1] = '\0';
    return out;
}

