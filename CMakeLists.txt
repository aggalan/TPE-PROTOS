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
        src/args.c
        src/args.h
        src/buffer.c
        src/buffer.h
        src/buffer_test.c
        src/main.c
        src/netutils.c
        src/netutils.h
        src/netutils_test.c
        src/parser.c
        src/parser.h
        src/parser_test.c
        src/parser_utils.c
        src/parser_utils.h
        src/parser_utils_test.c
        src/selector.c
        src/selector.h
        src/selector_test.c
        src/socks5.c
        src/socks5.h
        src/stm.c
        src/stm.h
        src/stm_test.c
        src/tests.h
        src/authentication/authentication.c
        src/authentication/authentication.h
        src/authentication/authentication_parser.c
        src/authentication/authentication_parser.h)
