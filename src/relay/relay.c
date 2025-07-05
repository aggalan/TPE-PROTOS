#include "relay/relay.h"
#include "metrics.h"

void relay_init(const unsigned state, struct selector_key *key) {
    LOG_INFO("Creating relay...\n");

    buffer_reset(&ATTACHMENT(key)->read_buffer);
    buffer_reset(&ATTACHMENT(key)->write_buffer);

    struct relay * data = &ATTACHMENT(key)->client.relay;
    data->fd = &ATTACHMENT(key)->client_fd;
    data->rb = &ATTACHMENT(key)->read_buffer;
    data->wb = &ATTACHMENT(key)->write_buffer;
    data->duplex = OP_READ | OP_WRITE;
    data->other = &ATTACHMENT(key)->origin.relay;
    selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_READ);
    data = &ATTACHMENT(key)->origin.relay;
    data->fd = &ATTACHMENT(key)->origin_fd;
    data->rb = &ATTACHMENT(key)->write_buffer;
    data->wb = &ATTACHMENT(key)->read_buffer;
    data->duplex = OP_READ | OP_WRITE;
    data->other = &ATTACHMENT(key)->client.relay;
    selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
    LOG_INFO("All relay elements created!\n");
    LOG_INFO("Client fd: %d, Origin fd: %d\n", ATTACHMENT(key)->client_fd, ATTACHMENT(key)->origin_fd);
    LOG_INFO("Relaying...\n");
}

static fd_interest copy_compute_interests(fd_selector s, struct relay *d) {
    fd_interest ret = OP_NOOP;

    if (*d->fd != -1) {
        if (((d->duplex & OP_READ) && buffer_can_write(d->rb))) {
            ret |= OP_READ;
        }
        if ((d->duplex & OP_WRITE) && buffer_can_read(d->wb)) {
            ret |= OP_WRITE;
        }
        LOG_INFO("Setting interests for fd %d: duplex=0x%x, can_write_rb=%d, can_read_wb=%d, ret=0x%x\n", 
                *d->fd, d->duplex, buffer_can_write(d->rb), buffer_can_read(d->wb), ret);
        if (SELECTOR_SUCCESS != selector_set_interest(s, *d->fd, ret)) {
            abort();
        }
    }

    return ret;
}

unsigned relay_read(struct selector_key *key) {
    struct relay *data = &ATTACHMENT(key)->client.relay;
    data = *data->fd == key->fd ? data : data->other;
    size_t size;
    ssize_t n;
    buffer *b = data->rb;
    unsigned ret = RELAY;

    LOG_INFO("relay_read called for fd %d, buffer can_write: %d\n", key->fd, buffer_can_write(b));
    
    uint8_t *ptr = buffer_write_ptr(b, &size);
    LOG_INFO("relay_read: Available buffer space: %zu bytes\n", size);
    
    n = recv(key->fd, ptr, size, 0);
    LOG_INFO("relay_read: recv returned %zd\n", n);

    if (n == 0) {
        LOG_INFO("Connection closed by peer on fd %d.\n", key->fd);
        shutdown(*data->fd, SHUT_RD);
        data->duplex &= ~OP_READ;
        if (*data->other->fd != -1) {
            shutdown(*data->other->fd, SHUT_WR);
            data->other->duplex &= ~OP_WRITE;
        }
    } else if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("Error reading from fd %d: %s\n", key->fd, strerror(errno));
            shutdown(*data->fd, SHUT_RD);
            data->duplex &= ~OP_READ;
            if (*data->other->fd != -1) {
                shutdown(*data->other->fd, SHUT_WR);
                data->other->duplex &= ~OP_WRITE;
            }
            return DONE; // Error handling
        }
        LOG_INFO("relay_read: No data available (EAGAIN/EWOULDBLOCK) for fd %d\n", key->fd);
        n = 0;
    } else {
        buffer_write_adv(b, n);
        metrics_add_bytes(n);
        LOG_INFO("relay_read: Read %zd bytes from fd %d\n", n, key->fd);
    }

    copy_compute_interests(key->s, data);
    copy_compute_interests(key->s, data->other);
    if (data->duplex == OP_NOOP) {
        ret = DONE;
    }

    return ret;
}

unsigned relay_write(struct selector_key *key) {
    struct relay *data = &ATTACHMENT(key)->client.relay;
    data = *data->fd == key->fd ? data : data->other;
    size_t size;
    ssize_t n;
    buffer *b = data->wb;
    unsigned ret = RELAY;

    uint8_t *ptr = buffer_read_ptr(b, &size);
    n = send(key->fd, ptr, size, MSG_NOSIGNAL);
    if (n == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("Error writing to fd %d: %s\n", key->fd, strerror(errno));
            shutdown(*data->fd, SHUT_WR);
            data->duplex &= ~OP_WRITE;
            if (*data->other->fd != -1) {
                shutdown(*data->other->fd, SHUT_RD);
                data->other->duplex &= ~OP_READ;
            }
            return DONE;
        }
        n = 0;
    } else {
        buffer_read_adv(b, n);
        metrics_add_bytes(n);

        if (buffer_can_read(b) == 0 && !(data->other->duplex & OP_READ)) {
            // If the buffer is empty and the other side is not interested in reading, we can close
            LOG_INFO("Buffer empty, closing connection.\n");
            shutdown(*data->fd, SHUT_WR);
            data->duplex &= ~OP_WRITE;
        } 
    }

    copy_compute_interests(key->s, data);
    copy_compute_interests(key->s, data->other);

    if (data->duplex == OP_NOOP) {
        ret = DONE;
    }

    return ret;
}
void relay_close(struct selector_key *key);