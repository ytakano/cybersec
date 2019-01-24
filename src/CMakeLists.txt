cmake_minimum_required(VERSION 3.9)

# build options
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "release")
endif()

set(CMAKE_C_FLAGS "-Wall")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# build targets
FILE(GLOB CSources *.c poptrie/*.c)
FILE(GLOB CHeaders *.h poptrie/*.h)
FILE(GLOB CMain main.c)

#list(REMOVE_ITEM CSources ${CMain})

add_executable(mytcpip ${CSources})