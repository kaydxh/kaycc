cmake_minimum_required(VERSION 2.8)
project(kaycc)

set(CXX_FLAGS
 -g
 # -DVALGRIND
 # -DMUDUO_STD_STRING
 -DCHECK_PTHREAD_RETURN_VALUE
 -D_FILE_OFFSET_BITS=64
 -Wall
 -Wextraa
 -Werror
 -Wconversion
 -Wno-unused-parameter
 -Wold-style-cast
 -Woverloaded-virtual
 -Wpointer-arith
 -Wshadow
 -Wwrite-strings
 -march=native
 # -MMD
 # -std=c++0x
 -rdynamic
 )


aux_source_directory(./base BASE)

add_library(${PROJECT_NAME} ${BASE})

target_link_libraries(${PROJECT_NAME} pthread)
#add_subdirectory(kaycc/base)
#add_subdirectory(kaycc/net)

