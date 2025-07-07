#ifndef METRICS_H
#define METRICS_H

#include <stddef.h>

struct metrics {
    unsigned long total_connections;
    unsigned long bytes_transferred;
    unsigned      concurrent_connections;
};

void metrics_init(void);
void metrics_connection_opened(void);
void metrics_connection_closed(void);
void metrics_add_bytes(size_t n);
struct metrics metrics_get(void);
char * metrics_to_string(void);
void log_metrics();

#endif // METRICS_H
