#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <libgen.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

static void prntmode(struct stat *stbuf, char * isFile) {
    if (S_ISDIR(stbuf->st_mode)) {
        printf("d");
    } else if (S_ISREG(stbuf->st_mode)) {
        printf("-");
        *isFile = 1;
    } else if (S_ISLNK(stbuf->st_mode)) {
        printf("l");
        *isFile = 2;
    } else {
        printf("?");
    }
    printf("%c",(stbuf->st_mode & S_IRUSR) ? 'r' : '-');
    printf("%c",(stbuf->st_mode & S_IWUSR) ? 'w' : '-');
    printf("%c",(stbuf->st_mode & S_IXUSR) ? 'x' : '-');
    printf("%c",(stbuf->st_mode & S_IRGRP) ? 'r' : '-');
    printf("%c",(stbuf->st_mode & S_IWGRP) ? 'w' : '-');
    printf("%c",(stbuf->st_mode & S_IXGRP) ? 'x' : '-');
    printf("%c",(stbuf->st_mode & S_IROTH) ? 'r' : '-');
    printf("%c",(stbuf->st_mode & S_IWOTH) ? 'w' : '-');
    printf("%c",(stbuf->st_mode & S_IXOTH) ? 'x' : '-');
}

static void prntuser(struct stat *stbuf) {
    struct passwd *pw;
    struct group *grp;
    pw = getpwuid(stbuf->st_uid);
    if (pw == NULL) {
        printf("%10d", stbuf->st_uid);
    } else {
        printf("%10s", pw->pw_name);
    }
    grp = getgrgid(stbuf->st_gid);
    if (grp == NULL) {
        printf("%10d", stbuf->st_gid);
    } else {
        printf("%10s", grp->gr_name);
    }
}

static void prntimes(struct stat *stbuf) {
    struct tm *tm = localtime(&stbuf->st_mtime);
    printf("  %04d-%02d-%02d %02d:%02d ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Not enough args\n");
        return -1;
    }
    for (int i = 1; i < argc; i++) {
        struct stat stbuf;
        char isFile = 0;
        if (lstat(argv[i], &stbuf) == -1) {
            perror(argv[i]);
            continue;
        }
        prntmode(&stbuf, &isFile);
        printf("%5lu\t", stbuf.st_nlink);
        prntuser(&stbuf);
        if (isFile==1) {
            printf("%12lli", (long long int)stbuf.st_size);
        } else {
            printf("            ");
        }
        prntimes(&stbuf);
        printf("%20s", basename(argv[i]));
        if (isFile == 2) {
            long path_max = pathconf(argv[i], _PC_SYMLINK_MAX);
            if (path_max == -1) {
                if (errno == 0) {
                    path_max = 1024;
                } else {
                    perror("pathconf failed");
                    continue;
                }
            }
            char target_path[path_max + 1];
            ssize_t len = readlink(argv[i], target_path, path_max);
            if (len != -1) {
                target_path[len] = '\0';
                printf(" -> %s", target_path);
            } else {
                perror(argv[i]);
                continue;
            }
            printf("\n");
        } else {
            printf("\n");
        }
    }
    return 0;
}