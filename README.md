# SISOP-4-2026-IT-053

## Soal 1 - Saya Asisten Kenz

---

### Deskripsi Soal

Pada soal ini, diminta untuk membuat sebuah filesystem berbasis FUSE bernama `kenz_rescue.c` yang bekerja sebagai filesystem mirror (passthrough) terhadap sebuah source directory bernama `amba_files/`.

Filesystem harus:
- Menampilkan seluruh file dari source directory ke mount directory.
- Menggunakan callback:
  - `getattr`
  - `readdir`
  - `open`
  - `read`
- Menambahkan sebuah file virtual bernama `tujuan.txt`.
- Isi `tujuan.txt` dibangkitkan secara dinamis dengan menggabungkan fragment setelah string `KOORD:` dari file `1.txt` hingga `7.txt`.

---

### Pengerjaan

#### Struktur Direktori

Struktur direktori yang digunakan:

```bash
soal_1/
├── amba_files/
│   ├── 1.txt
│   ├── 2.txt
│   ├── 3.txt
│   ├── 4.txt
│   ├── 5.txt
│   ├── 6.txt
│   └── 7.txt
├── kenz_rescue.c
└── mnt/
```

---

#### Header dan Library

Program menggunakan beberapa library utama seperti:
- `fuse3`
- `dirent`
- `fcntl`
- `unistd`
- `sys/stat`

```c
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
#include <sys/types.h>
```

---

#### Path Source Directory

Source directory disimpan secara global agar dapat diakses seluruh callback.

```c
static char source_dir[PATH_MAX];
```

Untuk membangun path asli file:

```c
static void build_path(char fullpath[PATH_MAX], const char *path)
{
    snprintf(fullpath, PATH_MAX, "%s%s", source_dir, path);
}
```

---

#### Callback getattr

Callback `getattr` digunakan untuk mengambil metadata file.

Jika file yang diakses adalah `tujuan.txt`, maka metadata dibuat secara virtual.

```c
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
```

---

#### Callback readdir

Callback `readdir` digunakan untuk membaca isi direktori.

Semua file dari source directory akan ditampilkan, lalu ditambahkan file virtual `tujuan.txt`.

```c
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
```

---

#### Callback open

Callback `open` digunakan untuk membuka file.

Jika file adalah `tujuan.txt`, maka langsung dianggap berhasil karena file bersifat virtual.

```c
static int kenz_open(const char *path, struct fuse_file_info *fi)
{
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
```

---

#### Fungsi generate_tujuan

Fungsi ini digunakan untuk membangkitkan isi file virtual `tujuan.txt`.

Program akan:
1. Membaca file `1.txt` hingga `7.txt`.
2. Mencari string `KOORD:`.
3. Mengambil fragment setelah `KOORD:`.
4. Menggabungkan seluruh fragment.
5. Menambahkan newline di akhir.

```c
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
```

---

#### Callback read

Callback `read` digunakan untuk membaca isi file.

Jika file adalah `tujuan.txt`, maka isi dibangkitkan secara dinamis menggunakan `generate_tujuan()`.

```c
static int kenz_read(const char *path, char *buf,
                     size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    (void) fi;

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
```

---

#### fuse_operations

Seluruh callback didaftarkan pada `fuse_operations`.

```c
static struct fuse_operations kenz_oper = {
    .getattr = kenz_getattr,
    .readdir = kenz_readdir,
    .open    = kenz_open,
    .read    = kenz_read,
};
```

---

#### Main Program

Program menerima:
- source directory
- mount directory

Program juga otomatis membuat mount directory jika belum ada.

```c
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
```

---

### Kompilasi Program

Program dikompilasi menggunakan:

```bash
gcc kenz_rescue.c -o kenz_rescue `pkg-config fuse3 --cflags --libs`
```

---

### Menjalankan Program

Mount filesystem:

```bash
./kenz_rescue amba_files mnt
```

---

### Testing

Melihat isi mount directory:

```bash
ls mnt
```

Membaca file virtual:

```bash
cat mnt/tujuan.txt
```

Melihat metadata file virtual:

```bash
stat mnt/tujuan.txt
```

---

### Unmount Filesystem

Filesystem dapat di-unmount menggunakan:

```bash
fusermount3 -u mnt
```

atau:

```bash
fusermount -u mnt
```

---

### Kendala

Kendala yang dialami:
- Warning `realpath()` akibat header `<stdlib.h>` belum ditambahkan.
- Warning `snprintf()` terkait kemungkinan overflow path.
- Penanganan file virtual agar ukuran file tetap konsisten saat diakses menggunakan `stat`.

Seluruh kendala berhasil diselesaikan sehingga filesystem dapat berjalan dengan baik.
