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
    [NEG_VERSION] = (parse_character) parse_version,  // Version is handled directly
    [NEG_NMETHODS] = (parse_character) parse_method_count, // Number of methods is handled directly
    [NEG_METHODS] = (parse_character) parse_methods,  // Methods are handled directly
    [NEG_END] = (parse_character) parse_end,  // End state does not require parsing
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
    LOG_DEBUG("Starting negotiation parse\n");
    if(parser == NULL || buffer == NULL) {
        return NEG_ERROR;
    }
    while(buffer_can_read(buffer)) {
        parser->state = parse_functions[parser->state](parser, buffer_read(buffer));
        
        // Break early if we reach end or error state
        if(parser->state == NEG_END || parser->state == NEG_ERROR) {
            break;
        }
    }
    return parser->state==NEG_END ? parse_end(parser,0):parser->state;
}

// -- State handlers ----------------------------------------------------------
NegState parse_version(NegParser * parser, uint8_t byte){
    LOG_DEBUG("Parsed version: %d (0x%02X)\n", byte, byte);
    LOG_DEBUG("Parser state: %d\n", parser->state);
    LOG_DEBUG("Expected SOCKS version: %d (0x%02X)\n", SOCKS_VERSION, SOCKS_VERSION);
    
    parser->version = byte;
    if(byte != SOCKS_VERSION){
        LOG_ERROR("parse_version: Client tried negotiating invalid version %d (0x%02X), expected %d (0x%02X)\n", 
                  byte, byte, SOCKS_VERSION, SOCKS_VERSION);
        return NEG_ERROR;
    }
    return NEG_NMETHODS;
}

NegState parse_method_count(NegParser * parser, uint8_t byte){
    LOG_DEBUG("Number of methods parsed: %d\n", byte);
    parser->nmethods = byte;
    
    if(byte == 0){
        LOG_DEBUG("parse_method_count: No authentication methods provided\n");
        return NEG_END;
    }
    return NEG_METHODS;
}

NegState parse_methods(NegParser * parser, uint8_t byte){
    LOG_DEBUG("Method parsed: %d\n", byte);
    
    // Priority: USER_PASS > NO_AUTH
    if(byte == USER_PASS){
        parser->auth_method = byte;
    }
    else if(byte==NO_AUTH && parser->auth_method!=USER_PASS && !socks5args.authentication_enabled){
        parser->auth_method=byte;
    }
    
    parser->nmethods -= 1;
    LOG_DEBUG("Remaining methods to parse: %d\n", parser->nmethods);
    
    return parser->nmethods == 0 ? NEG_END : NEG_METHODS;
}

NegState parse_end(NegParser * parser, uint8_t byte){
    LOG_DEBUG("parse_end: Unexpected byte %d received in END state\n", byte);
    // END state should not process additional bytes
    // This might indicate a protocol violation or buffer issue
    return parser != NULL  && parser->state == NEG_END && byte==0;;
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
    if(parser == NULL || buffer == NULL) {
        return NEG_FULL_BUFFER;
    }
    
    if (!buffer_can_write(buffer)) {
        return NEG_FULL_BUFFER;
    }
    
    LOG_DEBUG("Filling negotiation answer...\n");
    
    // Write SOCKS version
    buffer_write(buffer, SOCKS_VERSION);
    
    // Write selected authentication method
    buffer_write(buffer, parser->auth_method);
    
    LOG_DEBUG("Negotiation answer: Version=%d, Method=%d\n", 
              SOCKS_VERSION, parser->auth_method);
    
    if (parser->auth_method == NO_METHOD) {
        LOG_DEBUG("No acceptable authentication method found\n");
        return NEG_INVALID_METHOD;
    }
    
    return NEG_OK;
}