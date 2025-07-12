#ifndef REQUEST_PARSER_H
#define REQUEST_PARSER_H
#include "../buffer/buffer.h"
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../selector/selector.h"
#define METHOD_SIZE 255
#define SOCKS_VERSION 0x05
#define PORT_BYTE_LENGHT 2
#define REQ_MAX_DN_LENGHT 0xFF

typedef enum cmd {
    CONNECT = 0x01,
    BIND = 0x02,
    UDP_ASSOCIATE = 0x03
} ReqCmd;

typedef enum atyp {
    IPV4 = 0x01,
    DOMAINNAME = 0x03,
    IPV6 = 0x04
} ReqAtyp;

typedef union address {
    struct in_addr ipv4;
    uint8_t domainname[REQ_MAX_DN_LENGHT + 1];
    struct in6_addr ipv6;
} ReqAddr;

typedef enum request_state {
    REQ_VERSION = 0,
    REQ_CMD,
    REQ_RSV,
    REQ_ATYP,
    REQ_DNLEN,
    REQ_DST_ADDR,
    REQ_DST_PORT,
    REQ_ERROR,
    REQ_END
} ReqState;

typedef enum request_status {
    REQ_SUCCEDED = 0,
    REQ_ERROR_GENERAL_FAILURE,
    REQ_ERROR_CONNECTION_NOT_ALLOWED,
    REQ_ERROR_NTW_UNREACHABLE,
    REQ_ERROR_HOST_UNREACHABLE,
    REQ_ERROR_CONNECTION_REFUSED,
    REQ_ERROR_TTL_EXPIRED,
    REQ_ERROR_COMMAND_NOT_SUPPORTED,
    REQ_ERROR_ADDRESS_TYPE_NOT_SUPPORTED,
} ReqStatus;

typedef struct request_parser {
    uint8_t version;
    ReqCmd cmd;
    ReqAtyp atyp;
    uint8_t dnlen;
    ReqAddr dst_addr;
    in_port_t dst_port;
    ReqState state;
    ReqStatus status;
    uint8_t buf[16];   // buffer temporal reutilizable (16 bytes para IPv6, suficiente para dirección y puerto)
    uint8_t buf_idx;   // índice de progreso en el buffer
} ReqParser;

typedef enum request_codes {
    REQ_OK = 0,
    REQ_FULLBUFFER,
    REQ_UNSUPORTED_ATYP,
} ReqCodes;

/**
 * @brief Converts a request parser to a human-readable string
 * 
 * This function creates a string representation of the parsed SOCKS request,
 * including the command type and destination address.
 * 
 * @param p Pointer to the request parser structure
 * @return A string describing the request (e.g., "Command CONNECT to: 192.168.1.1:80")
 * @note The returned string is static and will be overwritten on subsequent calls
 */
const char* request_to_string(const ReqParser * parser);

/**
 * @brief Initializes a request parser structure
 * 
 * Sets all fields to zero and initializes the parser to the REQ_VERSION state.
 * This function should be called before parsing any SOCKS request.
 * 
 * @param p Pointer to the request parser structure to initialize
 * @note If p is NULL, the function returns without doing anything
 */
void init_request_parser(ReqParser * parser);

/**
 * @brief Parses SOCKS request data from a buffer
 * 
 * This function implements a state machine that parses SOCKS request data
 * byte by byte. It processes data from the input buffer and updates the
 * parser state accordingly.
 * 
 * @param p Pointer to the request parser structure
 * @param buffer Pointer to the input buffer containing SOCKS request data
 * @return Current parsing state (REQ_END on success, REQ_ERROR on failure)
 * @note The function processes all available data in the buffer
 */
ReqState request_parse(ReqParser* parser, buffer* buffer);

/**
 * @brief Checks if request parsing has completed successfully
 * 
 * @param p Pointer to the request parser structure
 * @return true if parsing has reached the REQ_END state, false otherwise
 */
bool has_request_read_ended(ReqParser * parser);

/**
 * @brief Checks if request parsing encountered an error
 * 
 * @param p Pointer to the request parser structure
 * @return true if parser is NULL or in REQ_ERROR state, false otherwise
 */
bool has_request_errors(ReqParser * parser);

/**
 * @brief Fills a buffer with a SOCKS reply message
 * 
 * This function constructs a SOCKS reply message based on the parsed request
 * and the current connection status. The reply includes the SOCKS version,
 * status code, reserved byte, address type, and bound address/port.
 * 
 * @param p Pointer to the request parser structure
 * @param buffer Pointer to the output buffer where the reply will be written
 * @param key Pointer to the selector key containing connection information
 * @return REQ_OK on success, REQ_FULLBUFFER if buffer is full, REQ_UNSUPORTED_ATYP if address type not supported
 * @note The function writes the reply in network byte order
 */
ReqCodes fill_request_answer(ReqParser * parser, buffer* buffer, struct selector_key * key);

#endif //REQUEST_PARSER_H
