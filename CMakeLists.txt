cmake_minimum_required(VERSION 3.12)
project(homicx C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
# set(CMAKE_CXX_STANDARD 17)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g -O0")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")


#find_library(RF24 rf24 REQUIRED)
#message(STATUS "using RF24 library: ${RF24}")


add_executable(homicx
   dep/argtable3/argtable3.c
   src/crc.c
   src/main.cpp
)

target_include_directories(homicx PRIVATE
   dep/argtable3
   dep/httplib
   dep
   src
)

target_link_libraries(homicx PUBLIC
#    ${RF24}
#    pthread
)
