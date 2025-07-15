#include "relay/relay.h"
#include "../metrics/metrics.h"

void relay_init(const unsigned state, struct selector_key *key) {
    LOG_DEBUG("Creating relay...\n");
    if(state != (unsigned int)8){
        LOG_DEBUG("[Relay]: Initiated with an invalid state: %u\n", state);
        return;
    }
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

    if (ATTACHMENT(key)->origin_fd != -1) {
        LOG_DEBUG("relay_init: File Descriptor IS valid!");
        selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
    }
    LOG_DEBUG("All relay elements created!\n");
    LOG_DEBUG("Client fd: %d, Origin fd: %d\n", ATTACHMENT(key)->client_fd, ATTACHMENT(key)->origin_fd);
    LOG_DEBUG("Relaying...\n");
    LOG_DEBUG("RELAYING...\n");
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
        LOG_DEBUG("Setting interests for fd %d: duplex=0x%x, can_write_rb=%d, can_read_wb=%d, ret=0x%x\n",
                *d->fd, d->duplex, buffer_can_write(d->rb), buffer_can_read(d->wb), ret);
        if (SELECTOR_SUCCESS != selector_set_interest(s, *d->fd, ret)) {
            abort();
        }
    }

    return ret;
}

static bool try_immediate_write(struct selector_key *key, struct relay *read_relay) {
    struct relay *write_relay = read_relay->other;
    (void)key;
    // Check if other side can write and has data to write
    if (*write_relay->fd == -1 ||
        !(write_relay->duplex & OP_WRITE) ||
        !buffer_can_read(write_relay->wb)) {
        return false;
    }

    size_t size;
    ssize_t n;
    buffer *b = write_relay->wb;

    uint8_t *ptr = buffer_read_ptr(b, &size);
    n = send(*write_relay->fd, ptr, size, MSG_NOSIGNAL | MSG_DONTWAIT);

    if (n == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            // Write would block, let selector handle it
            LOG_DEBUG("Immediate write would block on fd %d\n", *write_relay->fd);
            return false;
        } else {
            // Real error occurred
            LOG_DEBUG("Immediate write error on fd %d: %s\n", *write_relay->fd, strerror(errno));
            shutdown(*write_relay->fd, SHUT_WR);
            write_relay->duplex &= ~OP_WRITE;
            if (*read_relay->fd != -1) {
                shutdown(*read_relay->fd, SHUT_RD);
                read_relay->duplex &= ~OP_READ;
            }
            return true; // Handled the error
        }
    } else if (n > 0) {
        buffer_read_adv(b, n);
        metrics_add_bytes(n);
        LOG_DEBUG("Immediate write: Wrote %zd bytes to fd %d\n", n, *write_relay->fd);
        return true; // Successfully wrote data
    }

    return false;
}


unsigned relay_read(struct selector_key *key) {
    struct relay *data = &ATTACHMENT(key)->client.relay;
    data = *data->fd == key->fd ? data : data->other;
    size_t size;
    ssize_t n;
    buffer *b = data->rb;
    unsigned ret = RELAY;

    LOG_DEBUG("relay_read called for fd %d, buffer can_write: %d\n", key->fd, buffer_can_write(b));

    uint8_t *ptr = buffer_write_ptr(b, &size);
    LOG_DEBUG("relay_read: Available buffer space: %zu bytes\n", size);

    n = recv(key->fd, ptr, size, 0);
    LOG_DEBUG("relay_read: recv returned %zd\n", n);

    if (n == 0) {
        LOG_DEBUG("Connection closed by peer on fd %d.\n", key->fd);
        shutdown(*data->fd, SHUT_RD);
        data->duplex &= ~OP_READ;
        if (*data->other->fd != -1) {
            shutdown(*data->other->fd, SHUT_WR);
            data->other->duplex &= ~OP_WRITE;
        }
    } else if (n > 0) {
        buffer_write_adv(b, n);
        metrics_add_bytes(n);
        LOG_DEBUG("relay_read: Read %zd bytes from fd %d\n", n, key->fd);

        // Try immediate write to other side
        if (try_immediate_write(key, data)) {
            LOG_DEBUG("Immediate write succeeded or handled error\n");
        }
    } else {
        // n < 0, error occurred
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            LOG_DEBUG("relay_read: Read would block on fd %d\n", key->fd);
        } else {
            LOG_DEBUG("relay_read: Read error on fd %d: %s\n", key->fd, strerror(errno));
            shutdown(*data->fd, SHUT_RD);
            data->duplex &= ~OP_READ;
            if (*data->other->fd != -1) {
                shutdown(*data->other->fd, SHUT_WR);
                data->other->duplex &= ~OP_WRITE;
            }
        }
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

    if (!buffer_can_read(b)) {
        LOG_DEBUG("relay_write: No data to write on fd %d\n", key->fd);
        copy_compute_interests(key->s, data);
        copy_compute_interests(key->s, data->other);
        return ret;
    }

    uint8_t *ptr = buffer_read_ptr(b, &size);
    n = send(key->fd, ptr, size, MSG_NOSIGNAL);

    if (n == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            LOG_DEBUG("relay_write: Write would block on fd %d\n", key->fd);
        } else {
            LOG_DEBUG("relay_write: Write error on fd %d: %s\n", key->fd, strerror(errno));
            shutdown(*data->fd, SHUT_WR);
            data->duplex &= ~OP_WRITE;
            if (*data->other->fd != -1) {
                shutdown(*data->other->fd, SHUT_RD);
                data->other->duplex &= ~OP_READ;
            }
        }
    } else if (n > 0) {
        buffer_read_adv(b, n);
        metrics_add_bytes(n);
        LOG_DEBUG("relay_write: Wrote %zd bytes to fd %d\n", n, key->fd);

        // Try immediate read from other side
        if (buffer_can_write(data->other->rb) &&
            (data->other->duplex & OP_READ) &&
            *data->other->fd != -1) {

            size_t read_size;
            uint8_t *read_ptr = buffer_write_ptr(data->other->rb, &read_size);
            ssize_t read_n = recv(*data->other->fd, read_ptr, read_size, MSG_DONTWAIT);

            if (read_n > 0) {
                buffer_write_adv(data->other->rb, read_n);
                metrics_add_bytes(read_n);
                LOG_DEBUG("Immediate read: Read %zd bytes from fd %d\n", read_n, *data->other->fd);
            } else if (read_n == 0) {
                LOG_DEBUG("Immediate read: Connection closed by peer on fd %d\n", *data->other->fd);
                shutdown(*data->other->fd, SHUT_RD);
                data->other->duplex &= ~OP_READ;
                shutdown(*data->fd, SHUT_WR);
                data->duplex &= ~OP_WRITE;
            }
        }
    }

    copy_compute_interests(key->s, data);
    copy_compute_interests(key->s, data->other);

    if (data->duplex == OP_NOOP) {
        ret = DONE;
    }

    return ret;
}
void relay_close(struct selector_key *key) {
    LOG_DEBUG("Closing relay for key fd %d\n", key->fd);

    // Clean up any remaining data in buffers if needed
    struct relay *client_relay = &ATTACHMENT(key)->client.relay;
    struct relay *origin_relay = &ATTACHMENT(key)->origin.relay;

    // Close file descriptors if they're still open
    if (*client_relay->fd != -1) {
        close(*client_relay->fd);
        *client_relay->fd = -1;
    }

    if (*origin_relay->fd != -1) {
        close(*origin_relay->fd);
        *origin_relay->fd = -1;
    }

    // Reset buffers
    buffer_reset(client_relay->rb);
    buffer_reset(client_relay->wb);
}