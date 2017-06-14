#include "mfs-mount.h"

char *database_path = NULL;

static struct fuse_operations mfs_fuse_operations = {
        .getattr    = mfs_fuse_getattr,
        .readdir    = mfs_fuse_readdir,
        .open       = mfs_fuse_open,
        .read       = mfs_fuse_read,
};

static int mfs_fuse_opt_proc(void* data, const char* arg, int key, struct fuse_args* outargs);

int main(int argc, char *argv[]) {
    // @todo Provide a help message.
    // This is the fuse-style parser for the arguments
    // after which the database name and mountpoint
    // should have been set
    struct fuse_args custom_args = FUSE_ARGS_INIT(argc, argv);
    if(0 != fuse_opt_parse(&custom_args, NULL, NULL, mfs_fuse_opt_proc)){
        exit(EXIT_FAILURE);
    }
    if (database_path == '\0') {
        fprintf(stderr, "Could not find database.\nUsage: mfs-mount fileindex mountpoint\n");
        return errno;
    }
    return fuse_main(custom_args.argc, custom_args.argv, &mfs_fuse_operations, NULL);
}

static int mfs_fuse_opt_proc(void* data, const char* arg, int key, struct fuse_args* outargs) {
    if (key == -2) {
        static int count = 0;
        // First argument is the path to the database.
        if (count == 0) {
            database_path = realpath(arg, NULL);
            count++;
            // Returning 0 removes this argument from the list.
            return 0;
        }
    }
    // Persist other arguments.
    return 1;
}


static int mfs_fuse_getattr(const char *path, struct stat *stbuf) {
    int res = 0;
    struct inode inode;
    memset(stbuf, 0, sizeof(struct stat));

    inode = get_inode_from_path(path);

    if (inode.id != 0) {
        stbuf->st_mode = inode.st_mode;
        stbuf->st_nlink = 2;
        stbuf->st_ino = inode.id;
        stbuf->st_uid = inode.st_uid;
        stbuf->st_gid = inode.st_gid;
        stbuf->st_size = inode.st_size;
        stbuf->st_atime = inode.st_atime;
        stbuf->st_mtime = inode.st_mtime;
        stbuf->st_ctime = inode.st_ctime;
    }
    else
        res = -ENOENT;

    return res;
}

static int mfs_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;
    int const list_size = 1000;

    // char *zErrMsg = 0;

    struct inode inode;
    // @TODO Implement a vector to make size dynamic.
    struct inode items[list_size];
    int i;
    for (i = 0; i < list_size; i++) {
        items[i].id = 0;
    }

    inode = get_inode_from_path(path);
    // Get contents of directory inode.
    get_inodes_from_dir(items, inode.id);
    i = 0;
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
    struct inode inode;
    inode = get_inode_from_path(path);

    if (inode.id == 0)
        return -ENOENT;

    if ((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    return 0;
}

static int mfs_fuse_read(const char *path, char *buf, size_t size, off_t offset,
                         struct fuse_file_info *fi) {
    (void) fi;

    struct inode inode;

    inode = get_inode_from_path(path);

    if (inode.id == 0)
        return -ENOENT;
    int i;
    for (i = 0; i < size; i++) {
        // Write null bytes to the buffer.
        buf[offset + i] = '\0';
    }
    return 0;
}

void get_inodes_from_dir(struct inode *items, __ino_t id) {
    // Init db.
    sqlite3 *db;
    int rc;
    rc = sqlite3_open_v2(database_path, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, NULL);

    // Get directory inode.
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    char sql[500];
    int loop = 0;
    struct inode inode;
    inode.id = 0;
    char *zErrMsg = 0;
    const char *tail;
    sprintf(sql, "SELECT st_ino, parent_st_ino, name, st_mode FROM FILESYSTEM WHERE parent_st_ino = %d", (int) id);
    sqlite3_stmt *sql_statement;
    rc = sqlite3_prepare_v2(db, sql, 1000, &sql_statement, &tail);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        exit(EXIT_FAILURE);
    }
    while (sqlite3_step(sql_statement) == SQLITE_ROW) {
        inode.id = (ino_t) sqlite3_column_int(sql_statement, 0);
        inode.pid = (ino_t) sqlite3_column_int(sql_statement, 1);
        strcpy(inode.name, sqlite3_column_text(sql_statement, 2));
        inode.st_mode = (__mode_t) sqlite3_column_int(sql_statement, 3);
        items[loop] = inode;
        loop++;
    }
    sqlite3_finalize(sql_statement);
    sqlite3_close(db);
}

struct inode get_inode_from_path(const char *path) {
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
            inode = lookup_inode(sub, &inode);
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
        inode = lookup_inode(sub, &inode);
    }
    return inode;
}

struct inode lookup_inode(char *dir, struct inode *parent_inode) {
    // Init db.
    sqlite3 *db;
    int rc;
    rc = sqlite3_open_v2(database_path, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, NULL);

    // Get directory inode.
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    struct inode inode;
    inode.id = 0;
    char *zErrMsg = 0;
    __ino_t parent_id = parent_inode->id;
    if (parent_id < 0) {
        parent_id = 0;
    }
    if (dir[0] == '\0') {
        dir[0] = '/';
        dir[1] = '\0';
    }
    #define SELECT_Q "SELECT st_ino, parent_st_ino, name, st_mode, st_uid, st_gid, st_size, st_atime, st_mtime, st_ctime FROM FILESYSTEM WHERE name = ? AND parent_st_ino = ?;"
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
        inode.st_mode = (__mode_t) sqlite3_column_int(sql_statement, 3);
        inode.st_uid = (__uid_t) sqlite3_column_int(sql_statement, 4);
        inode.st_gid = (__gid_t) sqlite3_column_int(sql_statement, 5);
        inode.st_size = (__off_t) sqlite3_column_int(sql_statement, 6);
        inode.st_atime = (__time_t) sqlite3_column_int(sql_statement, 7);
        inode.st_mtime = (__time_t) sqlite3_column_int(sql_statement, 8);
        inode.st_ctime = (__time_t) sqlite3_column_int(sql_statement, 9);
    }
    sqlite3_finalize(sql_statement);
    sqlite3_close(db);
    return inode;
}
