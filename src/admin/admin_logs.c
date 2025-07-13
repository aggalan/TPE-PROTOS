#include "admin_logs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void admin_log_record(const char *username,     /* "-" si no hay     */
                      const char *src_ip, uint16_t src_port,
                      const char *dst_host, uint16_t dst_port,
                      const char *stage,        /* "CONNECTED", "AUTH", … */
                      const char *result,       /* "OK" / "FAIL" / "-"    */
                      size_t bytes_up, size_t bytes_down,
                      uint32_t dur_ms){

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    FILE *f = fopen(USER_FILE, "a");
    if (!f) {
        printf("Error opening log file %s\n", USER_FILE);
        return;
    }

    fprintf(f, "%s %s %s %u %s %u %s %s %zu %zu %u\n",
            timestamp,
            username ? username : "-",
            src_ip ? src_ip : "-",
            src_port,
            dst_host ? dst_host : "-",
            dst_port,
            stage ? stage : "-",
            result ? result : "-",
            bytes_up,
            bytes_down,
            dur_ms);
    fclose(f);
}

size_t admin_log_tail(char *out, size_t out_sz, size_t max_lines){
    FILE *f = fopen(USER_FILE, "r");
    if (!f) {
        snprintf(out, out_sz, "Error opening log file %s\n", USER_FILE);
        return 0;
    }

    size_t lines = 0;
    size_t total_read = 0;
    char line[512];

    char **line_buffer = malloc(max_lines * sizeof(char*));
    if (!line_buffer) {
        fclose(f);
        snprintf(out, out_sz, "Memory allocation error\n");
        return 0;
    }

    size_t total_lines = 0;

    while (fgets(line, sizeof(line), f)) {
        if (total_lines < max_lines) {
            line_buffer[total_lines] = strdup(line);
        } else {
            free(line_buffer[0]);
            for (size_t i = 0; i < max_lines - 1; i++) {
                line_buffer[i] = line_buffer[i + 1];
            }
            line_buffer[max_lines - 1] = strdup(line);
        }
        total_lines++;
    }

    size_t lines_to_copy = (total_lines < max_lines) ? total_lines : max_lines;
    for (size_t i = 0; i < lines_to_copy; i++) {
        size_t len = strlen(line_buffer[i]);
        if (total_read + len < out_sz - 1) {
            strncpy(out + total_read, line_buffer[i], out_sz - total_read - 1);
            total_read += len;
        } else {
            break;
        }
        free(line_buffer[i]);
    }

    for (size_t i = lines_to_copy; i < max_lines && i < total_lines; i++) {
        free(line_buffer[i]);
    }
    free(line_buffer);

    out[total_read] = '\0';
    fclose(f);
    return total_read;
}

size_t admin_log_tail_user(char *out, size_t out_sz,
                           const char *user, size_t max_lines){
    FILE *f = fopen(USER_FILE, "r");
    if (!f) {
        snprintf(out, out_sz, "Error opening log file %s\n", USER_FILE);
        return 0;
    }

    size_t lines = 0;
    size_t total_read = 0;
    char line[512];
    char **matching_lines = malloc(max_lines * sizeof(char*));
    if (!matching_lines) {
        fclose(f);
        snprintf(out, out_sz, "Memory allocation error\n");
        return 0;
    }

    size_t matched_count = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, user) != NULL) {
            if (matched_count < max_lines) {
                matching_lines[matched_count] = strdup(line);
            } else {
                free(matching_lines[0]);
                for (size_t i = 0; i < max_lines - 1; i++) {
                    matching_lines[i] = matching_lines[i + 1];
                }
                matching_lines[max_lines - 1] = strdup(line);
            }
            matched_count++;
        }
    }

    size_t lines_to_copy = (matched_count < max_lines) ? matched_count : max_lines;
    for (size_t i = 0; i < lines_to_copy; i++) {
        size_t len = strlen(matching_lines[i]);
        if (total_read + len < out_sz - 1) {
            strncpy(out + total_read, matching_lines[i], out_sz - total_read - 1);
            total_read += len;
        } else {
            break;
        }
        free(matching_lines[i]);
    }

    for (size_t i = lines_to_copy; i < max_lines && i < matched_count; i++) {
        free(matching_lines[i]);
    }
    free(matching_lines);

    out[total_read] = '\0';
    fclose(f);
    return total_read;
}

void admin_log_clear(void){
    FILE *f = fopen(USER_FILE, "w");
    if (!f) {
        printf("Error opening log file %s for clearing\n", USER_FILE);
        return;
    }
    fclose(f);
}

void admin_log_format_entry(const char *log_line, char *formatted, size_t formatted_sz) {
    if (!log_line || !formatted) return;

    char timestamp[64], username[64], src_ip[64], dst_host[64], stage[32], result[16];
    uint16_t src_port, dst_port;
    size_t bytes_up, bytes_down;
    uint32_t dur_ms;

    int parsed = sscanf(log_line, "%63s %63s %63s %hu %63s %hu %31s %15s %zu %zu %u",
                        timestamp, username, src_ip, &src_port, dst_host, &dst_port,
                        stage, result, &bytes_up, &bytes_down, &dur_ms);

    if (parsed >= 11) {
        snprintf(formatted, formatted_sz,
                 "[%s] User: %s, %s:%u -> %s:%u, Stage: %s, Result: %s, Up: %zu, Down: %zu, Duration: %ums",
                 timestamp, username, src_ip, src_port, dst_host, dst_port,
                 stage, result, bytes_up, bytes_down, dur_ms);
    } else {
        strncpy(formatted, log_line, formatted_sz - 1);
        formatted[formatted_sz - 1] = '\0';
    }
}