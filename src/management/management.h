#ifndef TPE_PROTOS_MANAGMENT_H
#define TPE_PROTOS_MANAGMENT_H
#include <stdint.h>
#include "../selector/selector.h"

 void mgmt_accept(struct selector_key *key);
 void mgmt_read(struct selector_key *key);
 void mgmt_close(struct selector_key *key);



#endif //TPE_PROTOS_MANAGMENT_H
