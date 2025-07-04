#include "relay/relay.h"

void relay_init(const unsigned state, struct selector_key *key) {
    LOG_INFO("Creating relay...\n");
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
        if (SELECTOR_SUCCESS != selector_set_interest(s, *d->fd, ret)) {
            abort();
        }
    }

    return ret;
}

unsigned relay_read(struct selector_key *key) {
    LOG_INFO("Reading...\n");
    struct relay *data = &ATTACHMENT(key)->client.relay;
    data = *data->fd == key->fd ? data : data->other;
    size_t size;
    ssize_t n;
    buffer *b = data->rb;
    unsigned ret = RELAY;

    uint8_t *ptr = buffer_write_ptr(b, &size);
    n = recv(key->fd, ptr, size, 0);
    if (n <= 0) {
        shutdown(*data->fd, SHUT_RD);
        data->duplex &= ~OP_READ;
        if (*data->other->fd != -1) {
            shutdown(*data->other->fd, SHUT_WR);
            data->other->duplex &= ~OP_WRITE;
        }
    } else {
        buffer_write_adv(b, n);
    }

    copy_compute_interests(key->s, data);
    copy_compute_interests(key->s, data->other);
    if (data->duplex == OP_NOOP) {
        ret = DONE;
    }

    return ret;
}

unsigned relay_write(struct selector_key *key) {
    LOG_INFO("Writing...\n");
    struct relay *data = &ATTACHMENT(key)->client.relay;
    data = *data->fd == key->fd ? data : data->other;
    size_t size;
    ssize_t n;
    buffer *b = data->wb;
    unsigned ret = RELAY;

    uint8_t *ptr = buffer_read_ptr(b, &size);
    n = send(key->fd, ptr, size, MSG_NOSIGNAL);
    if (n == -1) {
        shutdown(*data->fd, SHUT_WR);
        data->duplex &= ~OP_WRITE;
        if (*data->other->fd != -1) {
            shutdown(*data->other->fd, SHUT_RD);
            data->other->duplex &= ~OP_READ;
        }
    } else {
        buffer_read_adv(b, n);
    }

    copy_compute_interests(key->s, data);
    copy_compute_interests(key->s, data->other);

    if (data->duplex == OP_NOOP) {
        ret = DONE;
    }

    return ret;
}
void relay_close(struct selector_key *key);