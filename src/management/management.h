// management.h
#ifndef TPE_PROTOS_MANAGEMENT_H
#define TPE_PROTOS_MANAGEMENT_H

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "selector/selector.h"
#define MGMT_TIMEOUT 30

#define MAX_UDP_PACKET 1024
#define MGMT_VERSION    1

// Method codes
enum {
    MGMT_LOGIN     = 1,
    MGMT_STATS     = 2,
    MGMT_ADDUSER   = 3,
    MGMT_DELUSER   = 4,
    MGMT_LISTUSERS = 5,
    MGMT_SETAUTH   = 6,
    MGMT_DUMP      = 7,
    MGMT_SEARCHLOGS= 8,
    MGMT_CLEARLOGS = 9,
    MGMT_LOGOUT    = 10
};

enum {
    MGMT_REQ           = 0,
    MGMT_OK_SIMPLE     = 20,
    MGMT_OK_WITH_DATA  = 21,
    MGMT_ERR_SYNTAX    = 40,
    MGMT_ERR_AUTH      = 41,
    MGMT_ERR_NOTFOUND  = 42,
    MGMT_ERR_INTERNAL  = 50
};

struct mgmt_hdr {
    uint8_t version;   // Protocol version
    uint8_t method;    // Command code
    uint8_t status;    // Status code
    uint16_t length;   // Payload length (bytes)
    uint8_t reserved;  // Reserved for future flags
};


void mgmt_udp_handle(struct selector_key *key);


#endif // TPE_PROTOS_MANAGEMENT_H
