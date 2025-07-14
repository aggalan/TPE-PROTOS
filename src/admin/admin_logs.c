#include "admin_logs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void _ts(char s[24])
{
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(s, 24, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

void log_access(const char *user,
                const char *src_ip, unsigned src_port,
                const char *dst,    unsigned dst_port,
                unsigned status)
{
    char ts[24], line[256];
    _ts(ts);

    snprintf(line, sizeof line,
             "%s\t%s\tA\t%s\t%u\t%s\t%u\t%u\n",
             ts, user, src_ip, src_port, dst, dst_port, status);

    FILE *f = fopen(ACCESS_LOG_FILE, "a");
    if (f) {
        fputs(line, f);
        fclose(f);
    }
    fputs(line, stdout);
}

char* dump_access(int n_logs)
{
    static char result[10240];
    static char lines[100][256];

    FILE *f = fopen(ACCESS_LOG_FILE, "r");
    if (!f) {
        return NULL;
    }

    char buf[256];
    int count = 0;

    while (fgets(buf, sizeof(buf), f) && count < n_logs && count < 100) {
        strcpy(lines[count], buf);
        count++;
    }
    fclose(f);

    result[0] = '\0';
    for (int i = 0; i < count; i++) {
        strcat(result, lines[i]);
    }

    return result;
}

char *search_access(const char *user)
{
    size_t len = strlen(user);
    FILE *f = fopen(ACCESS_LOG_FILE, "r");
    if (!f) return NULL;

    char line[512];
    char *result = calloc(1, 4096);
    size_t used = 0, count = 0;

    while (fgets(line, sizeof line, f)) {
        char *p = strchr(line, '\t'); if (!p) continue;
        char *q = strchr(p + 1, '\t'); if (!q) continue;

        if ((size_t)(q - (p + 1)) == len &&
            strncmp(p + 1, user, len) == 0)
        {
            size_t l = strlen(line);
            if (used + l >= 4096) break;
            memcpy(result + used, line, l);
            used += l;
            count++;
            if (count >= 5) break;
        }
    }

    fclose(f);
    return count > 0 ? result : (free(result), NULL);
}


void clean_logs(void)
{
    FILE *f = fopen(ACCESS_LOG_FILE, "w");
    if (f) {
        fclose(f);
    }
}