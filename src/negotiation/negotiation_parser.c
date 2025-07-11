#include "negotiation_parser.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "../logging/logger.h"
#include "args.h"


static const uint8_t SOCKS_VERSION = 0x05;
struct socks5args socks5args;


typedef NegState (*parse_character)(NegParser * parser, uint8_t byte);


parse_character parse_functions[] = {
    [NEG_VERSION] = (parse_character) parse_version, // Version is handled directly
    [NEG_NMETHODS] = (parse_character) parse_method_count, // Number of methods is handled directly
    [NEG_METHODS] = (parse_character) parse_methods, // Methods are handled directly
    [NEG_END] = (parse_character) parse_end, // End state does not require parsing
    [NEG_ERROR] = (parse_character) parse_end // Error state does not require parsing
};

void init_negotiation_parser(NegParser * parser) {
    if(parser == NULL) return;
    parser->state = NEG_VERSION;
    parser->auth_method = NO_METHOD;
    parser->version = 0;
    parser->nmethods = 0;
}

NegState negotiation_parse(NegParser * parser, buffer * buffer){
    LOG_DEBUG("Stating negotiation parse\n");
    if(parser == NULL || buffer == NULL) {
        return NEG_ERROR;
    }
    while(buffer_can_read(buffer)) {
        parser->state=parse_functions[parser->state](parser,buffer_read(buffer));
    }
    return parser->state;
}
NegState parse_version(NegParser * parser, uint8_t byte){
    LOG_DEBUG("Parsed version: %d\n",byte);
    if(byte!=SOCKS_VERSION){
        LOG_ERROR("parse_version: Client tried negotiating an invalid version.\n");
        return NEG_ERROR;
    }
    return NEG_NMETHODS;
}

NegState parse_method_count(NegParser * parser, uint8_t byte){
    LOG_DEBUG("Number of methods parsed: %d\n",byte);
    parser->nmethods=byte;
    if(byte==0){
        return NEG_END;
    }
    return NEG_METHODS;

}

NegState parse_methods(NegParser * parser, uint8_t byte){
    LOG_DEBUG("Method parsed: %d\n",byte);
    if(byte==USER_PASS){
        parser->auth_method=byte;
    }
    else if(byte==NO_AUTH && parser->auth_method!=USER_PASS && !socks5args.authentication_enabled){
        parser->auth_method=byte;
    }
    parser->nmethods-=1;
    return parser->nmethods == 0 ? NEG_END : NEG_METHODS;
}

NegState parse_end(NegParser * parser, uint8_t byte){
    if(byte==USER_PASS){
        LOG_DEBUG("parse_method_count: Server accepts %d : USER_PASS authentication method\n",byte);
        parser->auth_method=byte;
    }
    else if(byte==NO_AUTH && parser->auth_method!=USER_PASS){
        LOG_DEBUG("parse_method_count: Server accepts %d authentication method\n",byte);
        parser->auth_method=byte;
    }
    parser->nmethods-=1;
    LOG_DEBUG("parse_method_count: Server has %d authentication methods to go\n",parser->nmethods);
    return parser->nmethods == 0 ? NEG_END : NEG_METHODS;
}



bool has_negotiation_read_ended(NegParser * parser){
    if(parser == NULL) return false;

    return parser->state == NEG_END;
}

bool has_negotiation_errors(NegParser * parser){
    if(parser == NULL) return false;
    return parser->state == NEG_ERROR;
}

NegCodes fill_negotiation_answer(NegParser * parser, buffer * buffer){
    if (!buffer_can_write(buffer))
        return NEG_FULL_BUFFER;
    LOG_DEBUG("Filling negotiation answer...\n");
    buffer_write(buffer, SOCKS_VERSION);
    buffer_write(buffer, parser->auth_method);
    if (parser->auth_method == NO_METHOD)
        return NEG_INVALID_METHOD;
    return NEG_OK;
}