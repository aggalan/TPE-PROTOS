//
// Created by Agustin Galan on 26/06/2025.
//

#include "authentication.h"
#include "../socks5.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>


void authentication_init(const unsigned state,struct selector_key *key) {
    printf("Creating authentication...\n");
    SocksClient *socks = ATTACHMENT(key);
    if (socks == NULL) {
        return;
    }
    //init_authentication_parser(&socks->client.negotiation_parser);
    printf("All authentication elements created!\n");

}

unsigned authentication_read(struct selector_key *key) {
    return 0;
}