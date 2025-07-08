#include "metrics.h"
#include "../logging/logger.h"
#include <string.h>

static struct metrics m;

void metrics_init(void) {
    memset(&m, 0, sizeof(m));
}


void metrics_new_connection() {
    m.concurrent_connections += 1;
    m.historic_connections += 1;
}

void metrics_closed_connection() {
    if(m.concurrent_connections > 0)
        m.concurrent_connections -= 1;
}

void metrics_add_bytes(size_t amount) {
    m.bytes_transferred += amount;
}

char * metrics_to_string(void) {
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "Connections: %lu, Bytes: %lu, Concurrent: %u",
             m.historic_connections, m.bytes_transferred, m.concurrent_connections);
    return buffer;
}

