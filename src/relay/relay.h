
#ifndef RELAY_H
#define RELAY_H



void relay_init(const unsigned state, struct selector_key *key);
unsigned relay_read(struct selector_key *key);
unsigned relay_write(struct selector_key *key);
void relay_close(struct selector_key *key);


#endif //RELAY_H
