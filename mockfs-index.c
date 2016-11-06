
#include "mockfs-index.h"
#include <getopt.h>

/* Flag set by ‘--verbose’. */
static int verbose_flag;

/**
 * Callback for sqlite.
 */
static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    return 0;
}


int main(int argc, char *argv[]) {
    char *source_path = ".";
    char *output_file = "fileindex";

    int c;

    // Parse cli options.
    while (1) {
        static struct option long_options[] = {
                /* These options set a flag. */
                {"verbose", no_argument,       &verbose_flag, 1},
                {"brief",   no_argument,       &verbose_flag, 0},
                /* These options don’t set a flag.
                   We distinguish them by their indices. */
                {"source",  required_argument, 0, 's'},
                {"output",  required_argument, 0, 'o'},
                {0, 0, 0, 0}
        };
        int option_index = 0;
        c = getopt_long(argc, argv, "o:", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
            case 's':
                source_path = optarg;
            case 'o':
                output_file = optarg;
                break;
            case '?':
                printf("Usage: mockfs-index -o index");
        }
    }
    unlink(output_file);

    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    char sql[500];

    /* Open database */
    rc = sqlite3_open(output_file, &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    /* Create SQL statement */
    strcpy(sql, "CREATE TABLE FILESYSTEM("  \
         "name            TEXT                 NOT NULL," \
         "st_ino          UNSIGNED BIG INT     PRIMARY KEY," \
         "parent_st_ino   UNSIGNED BIG INT     NOT NULL," \
         "st_mode          INT                  NOT NULL," \
         "st_uid          INT                  NOT NULL," \
         "st_gid          INT                  NOT NULL," \
         "st_size          INT                  NOT NULL," \
         "st_atime          INT                  NOT NULL," \
         "st_mtime          INT                  NOT NULL," \
         "st_ctime          INT                  NOT NULL);");

    /* Execute SQL statement */
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        exit(EXIT_FAILURE);
    }

    // Define the callback as an inline function, so we can use the variables inside main's scope.
    int list(const char *pathname, const struct stat *status, int type, struct FTW *ftwbuf) {
        char parent_path[PATH_SIZE];
        char path[PATH_SIZE];
        static int first = 1;
        // Set the inode id of the root dir to 0, as to have a starting point.
        __ino_t parent_ino = 0;
        strcpy(parent_path, pathname);
        strcpy(path, pathname);
        parent_path[ftwbuf->base] = '\0';
        struct stat st;
        if (verbose_flag) {
            int i;
            for(i=1; i<ftwbuf->level;i++)
                printf("-");
            printf("%s\n", &pathname[ftwbuf->base]);
        }

        if (!first) {
            stat(parent_path, &st);
            parent_ino = st.st_ino;
        }
        if (strcpy(path, ".")) {
            path[0] = '/';
        }

        sqlite3_stmt* sql_statement = NULL;
        const char* pszUnused;
        rc = sqlite3_prepare_v2(db, INSERT_QUERY, -1, &sql_statement, &pszUnused);
        sqlite3_bind_int(sql_statement, 1, (int) status->st_ino);
        sqlite3_bind_int(sql_statement, 2, (int) parent_ino);
        sqlite3_bind_text(sql_statement, 3, &path[ftwbuf->base], -1, SQLITE_STATIC);
        sqlite3_bind_int(sql_statement, 4, (int) status->st_mode);
        sqlite3_bind_int(sql_statement, 5, (int) status->st_uid);
        sqlite3_bind_int(sql_statement, 6, (int) status->st_gid);
        sqlite3_bind_int(sql_statement, 7, (int) status->st_size);
        sqlite3_bind_int(sql_statement, 8, (int) status->st_atime);
        sqlite3_bind_int(sql_statement, 9, (int) status->st_mtime);
        sqlite3_bind_int(sql_statement, 10, (int) status->st_ctime);
        sqlite3_step (sql_statement);
        sqlite3_finalize (sql_statement);

        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            exit(EXIT_FAILURE);
        }
        first = 0;
        return 0;
    }

    // Start walking the filesystem.
    int flags = 0;
    nftw(source_path, list, 1, flags);

    return 0;
}