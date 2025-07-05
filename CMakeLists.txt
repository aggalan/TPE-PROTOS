cmake_minimum_required(VERSION 3.31)
project(TPE_PROTOS C)

set(CMAKE_C_STANDARD 11)

include_directories(src)
include_directories(src/negotiation)

add_executable(TPE_PROTOS
        src/negotiation/negotiation.c
        src/negotiation/negotiation.h
        src/negotiation/negotiation_parser.c
        src/negotiation/negotiation_parser.h
        src/args/args.c
        src/args/args.h
        src/buffer/buffer.c
        src/buffer/buffer.h
        src/buffer/buffer_test.c
        src/main.c
        src/netutils/netutils.c
        src/netutils/netutils.h
        src/netutils/netutils_test.c
        src/parser/parser.c
        src/parser/parser.h
        src/parser/parser_test.c
        src/parser/parser_utils.c
        src/parser/parser_utils.h
        src/parser/parser_utils_test.c
        src/selector/selector.c
        src/selector/selector.h
        src/selector/selector_test.c
        src/socks5/socks5.c
        src/socks5/socks5.h
        src/stm/stm.c
        src/stm/stm.h
        src/stm/stm_test.c
        src/socks5/tests.h
        src/authentication/authentication.c
        src/authentication/authentication.h
        src/authentication/authentication_parser.c
        src/authentication/authentication_parser.h
        src/request/request.c
        src/request/request_parser.c
        src/request/request.h
        src/request/request_parser.h
        src/relay/relay.c
        src/relay/relay.h)
