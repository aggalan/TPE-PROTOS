#ifndef METRICS_H
#define METRICS_H

#include <stddef.h>

struct metrics {
    unsigned long historic_connections;
    unsigned long bytes_transferred;
    unsigned      concurrent_connections;

};

void metrics_init(void);
void metrics_new_connection(void);
void metrics_closed_connection(void);
void metrics_add_bytes(size_t amount);
char * metrics_to_string(void);
int get_concurrent_connections(void);

#endif // METRICS_H
