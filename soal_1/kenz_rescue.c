#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>

static char source_dir[PATH_MAX];

static const char *virtual_content =
    "Ini adalah file virtual tujuan.txt\n";

static void build_path(char fullpath[PATH_MAX], const char *path)
{
    snprintf(fullpath, PATH_MAX, "%s%s", source_dir, path);
}

static void generate_tujuan(char *result, size_t maxlen)
{
    result[0] = '\0';

    strncat(result, "Tujuan Mas Amba: ",
            maxlen - strlen(result) - 1);

    for (int i = 1; i <= 7; i++) {

        char filepath[PATH_MAX];

        filepath[0] = '\0';

        strncat(filepath, source_dir,
                sizeof(filepath) - strlen(filepath) - 1);

        char num[16];
        snprintf(num, sizeof(num), "/%d.txt", i);

        strncat(filepath, num,
                sizeof(filepath) - strlen(filepath) - 1);

        FILE *fp = fopen(filepath, "r");

        if (!fp)
            continue;

        char line[1024];

        while (fgets(line, sizeof(line), fp)) {

            char *start = strstr(line, "KOORD:");

            if (!start)
                continue;

            start += strlen("KOORD:");

            while (*start == ' ')
                start++;

            char fragment[512];

            sscanf(start, "%[^\n]", fragment);

            strncat(result, fragment,
                    maxlen - strlen(result) - 1);
        }

        fclose(fp);
    }

    strncat(result, "\n",
            maxlen - strlen(result) - 1);
}

static int kenz_getattr(const char *path, struct stat *stbuf,
                        struct fuse_file_info *fi)
{
    (void) fi;

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/tujuan.txt") == 0) {

        char content[4096];
        generate_tujuan(content, sizeof(content));

        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(content);

        return 0;
    }

    char fullpath[PATH_MAX];
    build_path(fullpath, path);

    int res = lstat(fullpath, stbuf);

    if (res == -1)
        return -errno;

    return 0;
}

static int kenz_readdir(const char *path, void *buf,
                        fuse_fill_dir_t filler,
                        off_t offset,
                        struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags)
{
    (void) offset;
    (void) fi;
    (void) flags;

    char fullpath[PATH_MAX];
    build_path(fullpath, path);

    DIR *dp = opendir(fullpath);

    if (dp == NULL)
        return -errno;

    struct dirent *de;

    while ((de = readdir(dp)) != NULL) {
        struct stat st;

        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        if (filler(buf, de->d_name, &st, 0, 0))
            break;
    }

    closedir(dp);

    if (strcmp(path, "/") == 0) {
        filler(buf, "tujuan.txt", NULL, 0, 0);
    }

    return 0;
}

static int kenz_open(const char *path, struct fuse_file_info *fi)
{
    /* FILE VIRTUAL */
    if (strcmp(path, "/tujuan.txt") == 0)
        return 0;

    char fullpath[PATH_MAX];
    build_path(fullpath, path);

    int fd = open(fullpath, fi->flags);

    if (fd == -1)
        return -errno;

    close(fd);
    return 0;
}

static int kenz_read(const char *path, char *buf,
                     size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    (void) fi;

    /* FILE VIRTUAL */
    if (strcmp(path, "/tujuan.txt") == 0) {

        char content[4096];
        generate_tujuan(content, sizeof(content));

        size_t len = strlen(content);

        if (offset >= len)
            return 0;

        if (offset + size > len)
            size = len - offset;

        memcpy(buf, content + offset, size);

        return size;
    }

    char fullpath[PATH_MAX];
    build_path(fullpath, path);

    int fd = open(fullpath, O_RDONLY);

    if (fd == -1)
        return -errno;

    int res = pread(fd, buf, size, offset);

    if (res == -1)
        res = -errno;

    close(fd);
    return res;
}

static struct fuse_operations kenz_oper = {
    .getattr = kenz_getattr,
    .readdir = kenz_readdir,
    .open    = kenz_open,
    .read    = kenz_read,
};

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s <source_directory> <mount_directory>\n",
                argv[0]);
        return 1;
    }

    if (realpath(argv[1], source_dir) == NULL) {
        perror("realpath");
        return 1;
    }

    struct stat st = {0};

    if (stat(argv[2], &st) == -1) {
        mkdir(argv[2], 0755);
    }

    char *fuse_argv[2];
    fuse_argv[0] = argv[0];
    fuse_argv[1] = argv[2];

    return fuse_main(2, fuse_argv, &kenz_oper, NULL);
}