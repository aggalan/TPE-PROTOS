#include "metrics.h"
#include "../logging/logger.h"
#include <string.h>

static struct metrics m;

void metrics_init(void) {
    memset(&m, 0, sizeof(m));
}

void metrics_connection_opened(void) {
    m.total_connections++;
    m.concurrent_connections++;
}

void metrics_connection_closed(void) {
    if (m.concurrent_connections > 0) {
        m.concurrent_connections--;
    }
}

void metrics_add_bytes(size_t n) {
    m.bytes_transferred += n;
}

struct metrics metrics_get(void) {
    return m;
}
char * metrics_to_string() {
    static char buffer[256];
    struct metrics m = metrics_get();
    snprintf(buffer, sizeof(buffer),
             "Total connections: %lu, Bytes transferred: %lu, Concurrent connections: %u",
             m.total_connections, m.bytes_transferred, m.concurrent_connections);
    return buffer;
}

void log_metrics() {
    struct metrics m = metrics_get();
    LOG_INFO("Metrics summary:");
    LOG_INFO("  Total connections       : %lu", m.total_connections);
    LOG_INFO("  Bytes transferred       : %lu", m.bytes_transferred);
    LOG_INFO("  Concurrent connections  : %u",  m.concurrent_connections);
}
