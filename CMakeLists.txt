cmake_minimum_required(VERSION 2.8)
project(kaycc)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wno-deprecated  -W -Wall -pthread -Wno-missing-braces -Wparentheses")

set(CXX_FLAGS
# -g
 # -DVALGRIND
 # -DMUDUO_STD_STRING
 #-DCHECK_PTHREAD_RETURN_VALUE
 #-D_FILE_OFFSET_BITS=64
 -Wall
 #-Wextraa
 -Werror
 #-Wconversion
 #-Wno-unused-parameter
 #-Wold-style-cast
 #-Woverloaded-virtual
 #-Wpointer-arith
 #-Wshadow
 #-Wwrite-strings
 #-march=native
 # -MMD
   -std=c++11
 #-rdynamic
 )


aux_source_directory(./base BASE)
aux_source_directory(./net  NET)
aux_source_directory(./net/poller  POLLER)

add_library(${PROJECT_NAME} ${BASE} ${NET} ${POLLER})

target_link_libraries(${PROJECT_NAME} pthread)
#add_subdirectory(kaycc/base)
#add_subdirectory(kaycc/net)

