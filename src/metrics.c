#include "metrics.h"
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
