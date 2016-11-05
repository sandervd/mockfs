/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#include "mfs-mount.h"

static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";

static struct fuse_operations hello_oper = {
        .getattr    = mfs_getattr,
        .readdir    = mfs_readdir,
        .open        = mfs_open,
        .read        = mfs_read,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &hello_oper, NULL);
}



static int mfs_getattr(const char *path, struct stat *stbuf) {
    int res = 0;
    sqlite3 *db;
    int rc;
    struct inode inode;
    inode.id = -1;
    memset(stbuf, 0, sizeof(struct stat));

    rc = sqlite3_open("fileindex", &db);
    // Get directory inode.
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    if (strcmp(path, "/") != 0) {
        inode = get_inode_from_path(db, path + 1);
    }
    else {
        inode = get_inode_from_path(db, ".");
    }
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (strcmp(path, "/test_dir") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (strcmp(path, hello_path) == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(hello_str);
    } else
        res = -ENOENT;

    return res;
}

static int mfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    sqlite3 *db;
    // char *zErrMsg = 0;
    int rc;
    struct inode inode;
    struct inode items[200];
    for (int i = 0; i < 200; i++) {
        items[i].id = -1;
    }




    rc = sqlite3_open("fileindex", &db);



    // Get directory inode.
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }
    if (strcmp(path, "/") != 0) {
        inode = get_inode_from_path(db, path);
    }
    else {
        inode = get_inode_from_path(db, ".");
    }
    // Get contents of directory inode.
    get_inodes_from_dir(db, items, inode.id);
    int i = 0;
    while (i< 200 && items[i].id != -1) {
        filler(buf, items[i].name, NULL, 0);
        i++;
    }

    printf("\n\n\nret inode %d\n", inode.id);

    if (strcmp(path, "/") != 0) {
        return -ENOENT;
    }


    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    //filler(buf, hello_path + 1, NULL, 0);
    //filler(buf, "test_dir", NULL, 0);

    return 0;
}

void get_inodes_from_dir(sqlite3 *db, struct inode *items, int id) {
    char sql[500];
    int loop = 0;
    struct inode inode;
    inode.id = -1;
    int rc;
    char *zErrMsg = 0;
    const char *tail;
    sprintf(sql, "SELECT INO, P_INO, NAME, MODE FROM FILESYSTEM WHERE P_INO = %d", id);
    sqlite3_stmt *res;
    rc = sqlite3_prepare_v2(db, sql, 1000, &res, &tail);
    printf("SQL: %s\n", sql);
    // rc = sqlite3_exec(db, sql, callback, &inode, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        exit(EXIT_FAILURE);
    }
    while (sqlite3_step(res) == SQLITE_ROW) {
        inode.id = sqlite3_column_int(res, 0);
        inode.pid = sqlite3_column_int(res, 1);
        strcpy(inode.name, sqlite3_column_text(res, 2));
        inode.mode = sqlite3_column_int(res, 3);
        items[loop] = inode;
        loop++;
        printf("%d|", sqlite3_column_int(res, 0));
        printf("%d|", sqlite3_column_int(res, 1));
        printf("%s|", sqlite3_column_text(res, 2));
        printf("%u\n", sqlite3_column_int(res, 3));
    }
}

static int mfs_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, hello_path) != 0)
        return -ENOENT;

    if ((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    return 0;
}

static int mfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    size_t len;
    (void) fi;
    if (strcmp(path, hello_path) != 0)
        return -ENOENT;

    len = strlen(hello_str);
    if (offset < len) {
        if (offset + size > len)
            size = len - offset;
        memcpy(buf, hello_str + offset, size);
    } else
        size = 0;

    return size;
}


struct inode get_inode_from_path(sqlite3 *db, const char *path) {
    char sub[255];
    sub[0] = '\0';
    int i = 0;
    int j = 0;
    struct inode inode;
    inode.id = -1;
    while (path[i] != '\0') {
        // Full substring
        sub[j] = '\0';
        if (path[i] == '/') {
            if (j != 0) {
                inode = lookup_inode(db, sub, &inode);
                if (inode.id == -1) {
                    return inode;
                }
            }

            j = 0;
        } else {
            sub[j] = path[i];
            j++;
        }

        i++;
    }
    if (j != 0) {
        sub[j] = '\0';
        inode = lookup_inode(db, sub, &inode);
    }
    return inode;
}

//static int callback(int *inode, int argc, char **argv, char **azColName) {
//    int i;
//    // *inode = argv[0];
//    for (i = 0; i < argc; i++) {
//        inode = argv[i];
//        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
//    }
//    printf("inode: %d\n", inode);
//    return 0;
//}

struct inode lookup_inode(sqlite3 *db, char *dir, struct inode *parent_inode) {
    char sql[500];
    struct inode inode;
    inode.id = -1;
    int rc;
    char *zErrMsg = 0;
    const char *tail;
    int parent_id = parent_inode->id;
    if (parent_id < 0) {
        parent_id = 0;
    }

    printf("\npath: %s\n", dir);
    sprintf(sql, "SELECT INO, P_INO, NAME, MODE FROM FILESYSTEM WHERE NAME = \"%s\" AND P_INO = %d", dir, parent_id);
    sqlite3_stmt *res;
    rc = sqlite3_prepare_v2(db, sql, 1000, &res, &tail);
    printf("SQL: %s\n", sql);
    // rc = sqlite3_exec(db, sql, callback, &inode, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        exit(EXIT_FAILURE);
    }
    while (sqlite3_step(res) == SQLITE_ROW) {
        inode.id = sqlite3_column_int(res, 0);
        inode.pid = sqlite3_column_int(res, 1);
        strcpy(inode.name, sqlite3_column_text(res, 2));
        inode.mode = sqlite3_column_int(res, 3);
        printf("%d|", sqlite3_column_int(res, 0));
        printf("%d|", sqlite3_column_int(res, 1));
        printf("%s|", sqlite3_column_text(res, 2));
        printf("%u\n", sqlite3_column_int(res, 3));

        //rec_count++;
    }
    printf("%s\n", dir);
    return inode;
}

