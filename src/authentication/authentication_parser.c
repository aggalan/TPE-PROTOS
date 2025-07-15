#include "authentication_parser.h"
#include <string.h>
#include <stdio.h>
#include "../logging/logger.h"
#include "admin.h"
#define AUTH_VERSION_USER_PASS 0x01


typedef AuthState (*parse_character)(AuthParser *p, uint8_t c);

static AuthState parse_version(AuthParser * parser, uint8_t byte);
static AuthState parse_username_length(AuthParser * parser, uint8_t byte);
static AuthState parse_username(AuthParser * parser, uint8_t byte);
static AuthState parse_password_length(AuthParser * parser, uint8_t byte);
static AuthState parse_password(AuthParser * parser, uint8_t byte);
static AuthState parse_end(AuthParser * parser, uint8_t byte);
static AuthState parse_error(AuthParser * parser, uint8_t byte);

static parse_character parse_functions[] = {
    [AUTH_VERSION] = (parse_character) parse_version,
    [USER_LENGTH]         = (parse_character) parse_username_length,
    [USERNAME]        = (parse_character) parse_username,
    [PASS_LENGTH]         = (parse_character) parse_password_length,
    [PASSWORD]       = (parse_character) parse_password,
    [AUTH_END]     = (parse_character) parse_end,
    [AUTH_ERROR]   = (parse_character) parse_error
};

void init_authentication_parser(AuthParser *parser) {
    if (parser == NULL) return;
    memset(parser, 0, sizeof(*parser));
    parser->state       = AUTH_VERSION;
    parser->auth_check  = AUTH_FAILURE;
}

AuthState authentication_parse(AuthParser *parser, buffer *buffer) {
    LOG_DEBUG("Starting authentication parse\n");
    if (parser == NULL || buffer == NULL) return AUTH_ERROR;
    while(buffer_can_read(buffer) && parser->state != AUTH_END) {
        parser->state=parse_functions[parser->state](parser,buffer_read(buffer));
    }
    return (parser != NULL && parser->state == AUTH_END) ? AUTH_END : AUTH_ERROR;
}

AuthState parse_version(AuthParser * parser, uint8_t byte){
    if(parser->state!=(unsigned int)0 && parser->state!=(unsigned int)2 ){
        LOG_DEBUG("[Authentication]: parse_version Initiated with an invalid state: %u\n", parser->state);
        return AUTH_ERROR;
    }  
    LOG_DEBUG("Version parsed: %d\n",byte);
    LOG_DEBUG("Expected version: %d\n", AUTH_VERSION_USER_PASS);
    if(byte != AUTH_VERSION_USER_PASS){
        LOG_DEBUG("parse_version: Oof! Ouch! Version %d is invalid :/\n",byte);
        return AUTH_ERROR;
    }
    return USER_LENGTH;
}

AuthState parse_username_length(AuthParser * parser, uint8_t byte){
    LOG_DEBUG("Parsed username length: %d \n",byte);
    if(byte==0){
        return PASS_LENGTH;
    }
    parser->ulen=byte;
    return USERNAME;
}

AuthState parse_username(AuthParser * parser, uint8_t byte){
    parser->uname[parser->idx++] = byte;
    if(parser->idx == parser->ulen){
        parser->idx = 0;
        LOG_DEBUG("Username parsed successfully\n");
        return PASS_LENGTH;
    }
    return USERNAME;
}

AuthState parse_password_length(AuthParser * parser, uint8_t byte){
    LOG_DEBUG("Parsed password length: %d \n",byte);
    if(byte==0){
        return AUTH_END;
    }
    parser->plen=byte;
    return PASSWORD;

}

AuthState parse_password(AuthParser * parser, uint8_t byte){
    parser->passwd[parser->idx++] = byte;
    if(parser->idx == parser->plen){
        parser->idx = 0;
        LOG_DEBUG("Password parsed successfully\n");
        parser->auth_check = validate_user(parser->uname, parser->passwd,USER_FILE) ? AUTH_SUCCESS : AUTH_FAILURE;
        LOG_DEBUG("Checking Authentification: %d\n", parser->auth_check);
        return AUTH_END;
    }
    return PASSWORD;
}

AuthState parse_end(AuthParser * parser, uint8_t byte){
    LOG_DEBUG("parse_end: Authentification has ended. \n");
    LOG_DEBUG("Received byte: %d\n", byte);
    LOG_DEBUG("Parser state: %d\n", parser->state);
    return parser != NULL && parser->state == AUTH_END && byte==0;
}

AuthState parse_error(AuthParser * parser, uint8_t byte){
    LOG_DEBUG("parse_error: Error in Authentification. \n");
    LOG_DEBUG("Received byte: %d\n", byte);
    LOG_DEBUG("Parser state: %d\n", parser->state);
    byte = byte; //@TODO: Cambiar cuando se haga Error Handling
    if(parser == NULL) LOG_DEBUG("parse_error: Parser is NULL\n");
    return AUTH_ERROR;
}

bool has_authentication_read_ended(AuthParser *p) {
    return p != NULL && p->state == AUTH_END;
}

bool has_authentication_errors(AuthParser *p) {
    return p != NULL && p->state == AUTH_ERROR;
}

AuthCodes fill_authentication_answer(AuthParser *p, buffer *b) {
    if (!buffer_can_write(b))
        return AUTH_REPLY_FULL_BUFFER;
    LOG_DEBUG("Filling authentication answer\n");
    buffer_write(b, AUTH_VERSION_USER_PASS);
    buffer_write(b, (p->auth_check == AUTH_SUCCESS) ? 0x00 : 0x01); // REP
    return p->auth_check == AUTH_SUCCESS? AUTH_REPLY_OK : AUTH_REPLY_DENIED;
}
