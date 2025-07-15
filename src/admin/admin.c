// admin.c
#include "admin.h"

#include <limits.h>
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

    if (count >= MAX_GENERAL_USERS) {
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


char *admin_list_users(const char *user_file, size_t limit, size_t offset) {
    if (!user_file) return NULL;
    if (ensure_file_exists(user_file) != 0) return NULL;

    // load all names
    size_t cap = 16, count = 0;
    char **names = malloc(cap * sizeof(char*));
    if (!names) return NULL;

    FILE *f = fopen(user_file, "r");
    if (!f) { free(names); return NULL; }

    char u[256], p[256];
    while (fscanf(f, "%255s %255s", u, p) == 2) {
        if (count >= cap) {
            cap *= 2;
            char **tmp = realloc(names, cap * sizeof(char*));
            if (!tmp) break;
            names = tmp;
        }
        names[count++] = strdup(u);
    }
    fclose(f);

    size_t start = offset < count ? offset : count;
    size_t end = (limit > 0) ? start + limit : count;
    if (end > count) end = count;

    size_t out_cap = 256, out_len = 0;
    char *out = malloc(out_cap);
    if (!out) { for (size_t i = 0; i < count; i++) free(names[i]); free(names); return NULL; }
    out[0] = '\0';

    for (size_t i = start; i < end; i++) {
        size_t need = strlen(names[i]) + 1;
        if (out_len + need + 1 > out_cap) {
            out_cap = (out_len + need + 1) * 2;
            char *tmp = realloc(out, out_cap);
            if (!tmp) break;
            out = tmp;
        }
        strcat(out, names[i]);
        strcat(out, "\n");
        out_len = strlen(out);
    }

    for (size_t i = 0; i < count; i++) free(names[i]);
    free(names);

    if (out_len > 0 && out[out_len-1] == '\n') out[out_len-1] = '\0';
    return out;
}

bool validate_user(const char *username, const char *password, const char *user_file) {
    if (!username || !password || !user_file) return false;

    FILE *f = fopen(user_file, "r");
    if (f == NULL)
        return false;
    char fuser[USER_MAX_LEN + 1];
    char fpass[PASS_MAX + 1];

    while (fscanf(f, " %15s %15s", fuser, fpass) == 2) {
        if (strcmp(username, fuser) == 0 && strcmp(password, fpass) == 0) {
            fclose(f);
            return true;
        }
    }
    fclose(f);
    return false;
}
