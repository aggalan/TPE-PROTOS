// management.h
#ifndef TPE_PROTOS_MANAGEMENT_H
#define TPE_PROTOS_MANAGEMENT_H

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "selector/selector.h"


void mgmt_udp_handle(struct selector_key *key);


#endif // TPE_PROTOS_MANAGEMENT_H
