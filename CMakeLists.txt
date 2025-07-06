cmake_minimum_required(VERSION 3.16)
project(TPE_PROTOS C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -std=gnu99 -g")

#
# Collect all source files under src/, excluding unit tests ("*_test.c").
# This mirrors the Makefile behaviour so the CMake build produces the same
# executable.
#
file(GLOB_RECURSE ALL_SRCS "src/*.c")
set(PROD_SRCS "")
foreach(f ${ALL_SRCS})
    if (NOT f MATCHES "_test\\.c$")
        list(APPEND PROD_SRCS ${f})
    endif()
endforeach()

# Include every subdirectory in src so headers are found automatically.
file(GLOB SUBDIRS LIST_DIRECTORIES true "src" "src/*")
foreach(dir ${SUBDIRS})
    if (IS_DIRECTORY ${dir})
        include_directories(${dir})
    endif()
endforeach()

add_executable(TPE_PROTOS ${PROD_SRCS})
target_link_libraries(TPE_PROTOS pthread)
