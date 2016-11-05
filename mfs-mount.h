//
// Created by sander on 02.11.16.
//

#ifndef MOCKFS_MFS_MOUNT_H_H
#define MOCKFS_MFS_MOUNT_H_H

#define FUSE_USE_VERSION 26

#define _XOPEN_SOURCE 500

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sqlite3.h>

#endif //MOCKFS_MFS_MOUNT_H_H

struct inode lookup_inode(sqlite3 *db, char *dir, struct inode *parent_inode);

struct inode get_inode_from_path(sqlite3 *db, const char *path);

static int mfs_fuse_getattr(const char *path, struct stat *stbuf);
static int mfs_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi);
static int mfs_fuse_open(const char *path, struct fuse_file_info *fi);
static int mfs_fuse_read(const char *path, char *buf, size_t size, off_t offset,
                         struct fuse_file_info *fi);

void get_inodes_from_dir(sqlite3 *db, struct inode *items, int id);

struct inode {
    int id;
    int pid;
    char name[255];
    int mode;
};
