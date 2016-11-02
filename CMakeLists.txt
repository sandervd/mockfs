cmake_minimum_required(VERSION 3.6)
project(mockfs)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")

set(SOURCE_FILES mockfs-index.h mockfs-index.c)

add_executable(mockfs ${SOURCE_FILES})
target_link_libraries(mockfs sqlite3)