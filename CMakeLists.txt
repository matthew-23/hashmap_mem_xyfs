cmake_minimum_required(VERSION 3.10)
project(xyfs C)

set(CMAKE_C_STANDARD 99)

include_directories(
        ../../../usr/fuse
)

ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)
ADD_DEFINITIONS(-DFUSE_USE_VERSION=30)


link_libraries(
        fuse
        dl
)

add_executable(xyfs xyfs.c hashmap.h hashmap.c xyfs.h)