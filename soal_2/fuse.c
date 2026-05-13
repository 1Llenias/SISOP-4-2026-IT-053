#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <limits.h>
#include <time.h>

static const char *storage_path = "encrypted_storage";
static const unsigned char XOR_KEY = 0x76;

/* --- Utility XOR --- */
void xor_buffer(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] ^= XOR_KEY;
    }
}

/* --- Path Helper --- */
void get_real_path(char fpath[PATH_MAX], const char *path, int is_file) {
    if (strcmp(path, "/") == 0) {
        snprintf(fpath, PATH_MAX, "%s", storage_path);
    } else {
        if (is_file) {
            // Perhatikan penggunaan / di antara %s dan %s
            snprintf(fpath, PATH_MAX, "%s%s.enc", storage_path, path);
        } else {
            snprintf(fpath, PATH_MAX, "%s%s", storage_path, path);
        }
    }
}

/* --- Metadata & Directory --- */
static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    char fpath[PATH_MAX];
    int res;

    get_real_path(fpath, path, 0); // Cek sebagai folder
    res = lstat(fpath, stbuf);

    if (res == -1) {
        get_real_path(fpath, path, 1); // Cek sebagai file .enc
        res = lstat(fpath, stbuf);
    }

    if (res == -1) return -errno;
    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset; (void) fi; (void) flags;
    char fpath[PATH_MAX];
    get_real_path(fpath, path, 0);

    DIR *dp = opendir(fpath);
    if (dp == NULL) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        char name[NAME_MAX];
        strncpy(name, de->d_name, NAME_MAX);

        size_t len = strlen(name);
        if (len > 4 && strcmp(name + len - 4, ".enc") == 0) {
            name[len - 4] = '\0';
        }

        if (filler(buf, name, &st, 0, 0)) break;
    }
    closedir(dp);
    return 0;
}

/* --- File Operations --- */
static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    char fpath[PATH_MAX];
    get_real_path(fpath, path, 1);
    int fd = creat(fpath, mode);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    char fpath[PATH_MAX];
    get_real_path(fpath, path, 1);
    int fd = open(fpath, fi->flags);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    char fpath[PATH_MAX];
    get_real_path(fpath, path, 1);
    int fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res != -1) xor_buffer(buf, res); // Dekripsi

    close(fd);
    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    char fpath[PATH_MAX];
    get_real_path(fpath, path, 1);
    int fd = open(fpath, O_WRONLY);
    if (fd == -1) return -errno;

    char *enc_buf = malloc(size);
    memcpy(enc_buf, buf, size);
    xor_buffer(enc_buf, size); // Enkripsi

    int res = pwrite(fd, enc_buf, size, offset);
    free(enc_buf);
    close(fd);
    return res;
}

static int xmp_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    (void) fi;
    char fpath[PATH_MAX];
    get_real_path(fpath, path, 1);
    int res = truncate(fpath, size);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_unlink(const char *path) {
    char fpath[PATH_MAX];
    get_real_path(fpath, path, 1);
    int res = unlink(fpath);
    if (res == -1) return -errno;
    return 0;
}

/* --- Folder Operations --- */
static int xmp_mkdir(const char *path, mode_t mode) {
    char fpath[PATH_MAX];
    get_real_path(fpath, path, 0);
    int res = mkdir(fpath, mode);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_rmdir(const char *path) {
    char fpath[PATH_MAX];
    get_real_path(fpath, path, 0);
    int res = rmdir(fpath);
    if (res == -1) return -errno;
    return 0;
}

/* --- Permission & Time --- */
static int xmp_access(const char *path, int mask) {
    char fpath[PATH_MAX];
    get_real_path(fpath, path, 0);
    int res = access(fpath, mask);
    if (res == -1) {
        get_real_path(fpath, path, 1);
        res = access(fpath, mask);
    }
    if (res == -1) return -errno;
    return 0;
}

static int xmp_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    (void) fi;
    char fpath[PATH_MAX];
    get_real_path(fpath, path, 0);
    int res = utimensat(0, fpath, tv, AT_SYMLINK_NOFOLLOW);
    if (res == -1) {
        get_real_path(fpath, path, 1);
        res = utimensat(0, fpath, tv, AT_SYMLINK_NOFOLLOW);
    }
    if (res == -1) return -errno;
    return 0;
}

static const struct fuse_operations xmp_oper = {
    .getattr  = xmp_getattr,
    .readdir  = xmp_readdir,
    .mkdir    = xmp_mkdir,
    .rmdir    = xmp_rmdir,
    .create   = xmp_create,
    .open     = xmp_open,
    .read     = xmp_read,
    .write    = xmp_write,
    .truncate = xmp_truncate,
    .unlink   = xmp_unlink,
    .access   = xmp_access,
    .utimens  = xmp_utimens,
};

int main(int argc, char *argv[]) {
    umask(0);

    char absolute_storage_path[PATH_MAX];
    if (realpath("encrypted_storage", absolute_storage_path) == NULL) {
        // Jika folder belum ada, buat dulu
        mkdir("encrypted_storage", 0755);
        realpath("encrypted_storage", absolute_storage_path);
    }
    
    storage_path = strdup(absolute_storage_path);

    printf("FUSE server started. Storage path: %s\n", storage_path);
    return fuse_main(argc, argv, &xmp_oper, NULL);
}