#include "mfs-mount.h"

static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";

static struct fuse_operations mfs_fuse_operations = {
        .getattr    = mfs_fuse_getattr,
        .readdir    = mfs_fuse_readdir,
        .open       = mfs_fuse_open,
        .read       = mfs_fuse_read,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &mfs_fuse_operations, NULL);
}


static int mfs_fuse_getattr(const char *path, struct stat *stbuf) {
    int res = 0;
    sqlite3 *db;
    int rc;
    struct inode inode;
    memset(stbuf, 0, sizeof(struct stat));

    rc = sqlite3_open("fileindex", &db);
    // Get directory inode.
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    inode = get_inode_from_path(db, path);

    if (inode.id != 0) {
        stbuf->st_mode = inode.mode;
        stbuf->st_nlink = 2;
        stbuf->st_ino = inode.id;
    }
    else
        res = -ENOENT;

    return res;
}

static int mfs_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    sqlite3 *db;
    // char *zErrMsg = 0;
    int rc;
    struct inode inode;
    // @TODO Implement a vector to make size dynamic.
    struct inode items[200];
    for (int i = 0; i < 200; i++) {
        items[i].id = 0;
    }
    rc = sqlite3_open("fileindex", &db);

    // Get directory inode.
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }
    inode = get_inode_from_path(db, path);
    // Get contents of directory inode.
    get_inodes_from_dir(db, items, inode.id);
    int i = 0;
    while (i< 200 && items[i].id != 0) {
        filler(buf, items[i].name, NULL, 0);
        i++;
    }

    // Add self and parent pointer.
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    return 0;
}

static int mfs_fuse_open(const char *path, struct fuse_file_info *fi) {
    sqlite3 *db;
    // char *zErrMsg = 0;
    int rc;
    struct inode inode;
    rc = sqlite3_open("fileindex", &db);
    // Get directory inode.
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    if (inode.id == 0)
        return -ENOENT;

    if ((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    return 0;
}

static int mfs_fuse_read(const char *path, char *buf, size_t size, off_t offset,
                         struct fuse_file_info *fi) {
    size_t len;
    (void) fi;

    sqlite3 *db;
    // char *zErrMsg = 0;
    int rc;
    struct inode inode;
    rc = sqlite3_open("fileindex", &db);
    // Get directory inode.
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    inode = get_inode_from_path(db, path);


    if (inode.id == 0)
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

void get_inodes_from_dir(sqlite3 *db, struct inode *items, __ino_t id) {
    char sql[500];
    int loop = 0;
    struct inode inode;
    inode.id = 0;
    int rc;
    char *zErrMsg = 0;
    const char *tail;
    sprintf(sql, "SELECT st_ino, parent_st_ino, name, st_mode FROM FILESYSTEM WHERE parent_st_ino = %d", (int) id);
    sqlite3_stmt *res;
    rc = sqlite3_prepare_v2(db, sql, 1000, &res, &tail);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        exit(EXIT_FAILURE);
    }
    while (sqlite3_step(res) == SQLITE_ROW) {
        inode.id = (ino_t) sqlite3_column_int(res, 0);
        inode.pid = (ino_t) sqlite3_column_int(res, 1);
        strcpy(inode.name, sqlite3_column_text(res, 2));
        inode.mode = (__mode_t) sqlite3_column_int(res, 3);
        items[loop] = inode;
        loop++;
    }
}

struct inode get_inode_from_path(sqlite3 *db, const char *path) {
    char sub[255];
    sub[0] = '\0';
    int i = 0;
    int j = 0;
    struct inode inode;
    inode.id = 0;
    while (path[i] != '\0') {
        // Full substring
        sub[j] = '\0';
        if (path[i] == '/') {
            inode = lookup_inode(db, sub, &inode);
            // Not found.
            if (inode.id == 0) {
                return inode;
            }
            j = 0;
        }
        else {
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

struct inode lookup_inode(sqlite3 *db, char *dir, struct inode *parent_inode) {
    char sql[500];
    struct inode inode;
    inode.id = 0;
    int rc;
    char *zErrMsg = 0;
    const char *tail;
    __ino_t parent_id = parent_inode->id;
    if (parent_id < 0) {
        parent_id = 0;
    }
    if (dir[0] == '\0') {
        dir[0] = '/';
        dir[1] = '\0';
    }
    #define SELECT_Q "SELECT st_ino, parent_st_ino, name, st_mode FROM FILESYSTEM WHERE name = ? AND parent_st_ino = ?;"
    sqlite3_stmt* sql_statement = NULL;
    const char* pszUnused;
    rc = sqlite3_prepare_v2(db, SELECT_Q, -1, &sql_statement, &pszUnused);
    sqlite3_bind_text(sql_statement, 1, dir, -1, SQLITE_STATIC);
    sqlite3_bind_int(sql_statement, 2, (int) parent_id);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        exit(EXIT_FAILURE);
    }
    while (sqlite3_step(sql_statement) == SQLITE_ROW) {
        inode.id = (__ino_t) sqlite3_column_int(sql_statement, 0);
        inode.pid = (__ino_t) sqlite3_column_int(sql_statement, 1);
        strcpy(inode.name, sqlite3_column_text(sql_statement, 2));
        inode.mode = (__mode_t) sqlite3_column_int(sql_statement, 3);
    }
    sqlite3_finalize (sql_statement);
    return inode;
}
