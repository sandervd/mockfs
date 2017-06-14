cmake_minimum_required(VERSION 3.5)
project(mockfs)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")

set(SOURCE_FILES_INDEX mockfs-index.h mockfs-index.c)
set(SOURCE_FILES_MOUNT mfs-mount.c mfs-mount.h)

set(CMAKE_C_FLAGS "-D_FILE_OFFSET_BITS=64")
add_executable(mfs-index ${SOURCE_FILES_INDEX})
add_executable(mfs-mount ${SOURCE_FILES_MOUNT})
target_link_libraries(mfs-index sqlite3)
target_link_libraries(mfs-mount sqlite3)
target_link_libraries(mfs-mount fuse)
install(TARGETS mfs-index mfs-mount
        RUNTIME DESTINATION bin)
