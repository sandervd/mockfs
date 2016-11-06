//
// Created by sander on 02.11.16.
//

#ifndef MOCKFS_MOCKFS_INDEX_H
#define MOCKFS_MOCKFS_INDEX_H

#define _XOPEN_SOURCE 500

#include <ftw.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <getopt.h>

#define PATH_SIZE 255

#define INSERT_QUERY "INSERT INTO FILESYSTEM(st_ino,parent_st_ino,name,st_mode,st_uid,st_gid,st_size,st_atime,st_mtime,st_ctime) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"

#endif //MOCKFS_MOCKFS_INDEX_H

int handle(char *dir, int *parent_inode);
int get_inode_from_path(char *path);