// admin.c
#include "admin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TMP_SUFFIX ".tmp"

static int ensure_file_exists(const char *user_file) {
    FILE *f = fopen(user_file, "a");
    if (!f) return -1;
    fclose(f);
    return 0;
}

int admin_add_user(const char *username, const char *password, const char *user_file) {
    if (!username || !password || !user_file) return -1;
    if (ensure_file_exists(user_file) != 0) return -1;

    FILE *f = fopen(user_file, "r");
    if (!f) return -1;

    char line[512], u[256], p[256];
    size_t count = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%255s %255s", u, p) == 2) {
            if (strcmp(u, username) == 0) {
                fclose(f);
                return -1;
            }
            count++;
        }
    }
    fclose(f);

    if (strcmp(user_file, ADMIN_FILE) == 0 && count >= ADMIN_MAX) {
        return -1;
    }

    f = fopen(user_file, "a");
    if (!f) return -1;
    int ok = (fprintf(f, "%s %s\n", username, password) > 0) ? 0 : -1;
    fclose(f);
    return ok;
}

int admin_del_user(const char *username, const char *user_file) {
    if (!username || !user_file) return -1;
    if (ensure_file_exists(user_file) != 0) return -1;

    char tmp_name[512];
    snprintf(tmp_name, sizeof(tmp_name), "%s%s", user_file, TMP_SUFFIX);

    FILE *in = fopen(user_file, "r");
    if (!in) return -1;
    FILE *out = fopen(tmp_name, "w");
    if (!out) { fclose(in); return -1; }

    char line[512], u[256], p[256];
    int found = 0;
    while (fgets(line, sizeof(line), in)) {
        if (sscanf(line, "%255s %255s", u, p) == 2 && strcmp(u, username) == 0) {
            found = 1;
            continue;
        }
        fputs(line, out);
    }
    fclose(in);
    fclose(out);

    if (!found) {
        remove(tmp_name);
        return -1;
    }
    if (rename(tmp_name, user_file) != 0) {
        remove(tmp_name);
        return -1;
    }
    return 0;
}


char *admin_list_users(const char *user_file) {
    if (!user_file) return NULL;
    if (ensure_file_exists(user_file) != 0) return NULL;

    FILE *f = fopen(user_file, "r");
    if (!f) return NULL;

    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    if (!out) { fclose(f); return NULL; }
    out[0] = '\0';

    char line[512], u[256], p[256];
    size_t count = 0;
    size_t limit = (strcmp(user_file, ADMIN_FILE) == 0) ? ADMIN_MAX : USER_MAX;

    while (count < limit && fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%255s %255s", u, p) == 2) {
            size_t need = strlen(u) + 1;
            if (len + need + 1 > cap) {
                cap = (len + need + 1) * 2;
                char *tmp = realloc(out, cap);
                if (!tmp) { free(out); fclose(f); return NULL; }
                out = tmp;
            }
            memcpy(out + len, u, strlen(u));
            len += strlen(u);
            out[len++] = '\n';
            out[len] = '\0';
            count++;
        }
    }
    fclose(f);

    if (len > 0 && out[len-1] == '\n') out[len-1] = '\0';
    return out;
}

int validate_user(const char *username, const char *password, const char *user_file) {
    if (!username || !password || !user_file) return -1;
    if (ensure_file_exists(user_file) != 0) return -1;

    FILE *f = fopen(user_file, "r");
    if (!f) return -1;

    char line[512], u[256], p[256];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%255s %255s", u, p) == 2 && strcmp(u, username) == 0) {
            fclose(f);
            return (strcmp(p, password) == 0) ? 0 : -1;
        }
    }
    fclose(f);
    return -1;
}


