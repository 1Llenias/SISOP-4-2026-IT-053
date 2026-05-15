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

## Soal 2 - Poke MOO

---

### Deskripsi Soal

Pada soal ini, praktikan diminta untuk membuat sebuah filesystem virtual menggunakan FUSE (Filesystem in Userspace) yang berfungsi sebagai translator antara direktori mount dan direktori penyimpanan terenkripsi.

Filesystem ini memiliki mekanisme:
- Semua file yang disimpan pada direktori asli (`encrypted_storage`) akan disimpan dalam keadaan terenkripsi XOR dengan key `0x76`
- Semua file yang diakses melalui mount point (`fuse_mount`) akan otomatis didekripsi
- Nama file pada direktori asli memiliki tambahan ekstensi `.enc`
- Operasi filesystem dilakukan menggunakan FUSE operations

Selain itu, sistem juga diintegrasikan dengan Docker untuk menjalankan mini database service menggunakan bind mount ke filesystem FUSE.

---

### Struktur Direktori

```text
soal_2/
├── client.c
├── server
├── fuse.c
├── Dockerfile
├── encrypted_storage/
└── fuse_mount/
```

---

### Pengerjaan FUSE

#### Konsep Dasar

Filesystem ini bekerja sebagai translator:

```text
User
 ↓
fuse_mount
 ↓
FUSE
 ↓
encrypted_storage
```

Ketika user mengakses file dari `fuse_mount`, isi file akan otomatis didekripsi menggunakan XOR.

Sebaliknya, ketika user menulis file melalui `fuse_mount`, file akan dienkripsi sebelum disimpan pada `encrypted_storage`.

---

### Implementasi XOR Encryption

praktikan menggunakan XOR dengan key `0x76`.

```c
static const unsigned char XOR_KEY = 0x76;
```

Fungsi XOR:

```c
void xor_buffer(char *buf, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        buf[i] ^= XOR_KEY;
    }
}
```

Karena XOR bersifat reversible, fungsi yang sama dapat digunakan untuk:
- encrypt
- decrypt

---

### Path Translation

Filesystem memiliki dua bentuk path:

| Mount Point | Real Storage |
|---|---|
| `/halo.txt` | `encrypted_storage/halo.txt.enc` |

Untuk itu dibuat fungsi translator path:

```c
void get_real_path(char fpath[PATH_MAX],
                   const char *path,
                   int encrypted)
{
    sprintf(fpath, "%s%s", storage_path, path);

    if (encrypted && strcmp(path, "/") != 0)
    {
        strcat(fpath, ".enc");
    }
}
```

Parameter:
- `encrypted = 1` → tambahkan `.enc`
- `encrypted = 0` → direktori biasa

---

### Implementasi FUSE Operations

Filesystem mengimplementasikan operasi berikut:

| Operation | Fungsi |
|---|---|
| getattr | Mengambil atribut file |
| readdir | Membaca isi direktori |
| mkdir | Membuat direktori |
| rmdir | Menghapus direktori |
| create | Membuat file |
| open | Membuka file |
| read | Membaca file |
| write | Menulis file |
| truncate | Mengubah ukuran file |
| unlink | Menghapus file |
| access | Mengecek akses |
| utimens | Mengubah timestamp |

---

### getattr

Operation ini digunakan untuk membaca atribut file/direktori.

```c
static int xmp_getattr(const char *path,
                       struct stat *stbuf,
                       struct fuse_file_info *fi)
{
    (void) fi;

    int res;
    char fpath[PATH_MAX];

    get_real_path(fpath, path, 0);
    res = lstat(fpath, stbuf);

    if (res == -1) {
        get_real_path(fpath, path, 1);
        res = lstat(fpath, stbuf);
    }

    if (res == -1)
        return -errno;

    return 0;
}
```

Sistem akan:
1. mencoba membaca sebagai direktori biasa
2. jika gagal, mencoba membaca sebagai file `.enc`

Metode ini memperbaiki bug mount root (`/`) yang sebelumnya menyebabkan FUSE gagal melakukan mount.

---

### readdir

Digunakan untuk membaca isi direktori.

```c
static int xmp_readdir(...)
```

Pada operasi ini:
- isi direktori asli dibaca dari `encrypted_storage`
- ekstensi `.enc` disembunyikan dari user

Contoh:

```text
encrypted_storage/test.txt.enc
```

akan terlihat sebagai:

```text
fuse_mount/test.txt
```

---

### read

Digunakan untuk membaca file.

```c
static int xmp_read(...)
```

Langkah:
1. membaca file `.enc`
2. melakukan XOR decrypt
3. mengirim hasil ke user

```c
xor_buffer(buf, res);
```

---

### write

Digunakan untuk menulis file.

```c
static int xmp_write(...)
```

Langkah:
1. menerima data dari user
2. melakukan XOR encrypt
3. menyimpan ke file `.enc`

```c
xor_buffer(enc_buf, size);
```

---

### Automatic Directory Creation

Filesystem akan otomatis membuat:
- `encrypted_storage`
- `fuse_mount`

jika belum tersedia.

```c
if (stat("encrypted_storage", &st) == -1)
{
    mkdir("encrypted_storage", 0755);
}
```

---

### Penggunaan FUSE

#### Compile

```bash
gcc fuse.c `pkg-config fuse3 --cflags --libs` -o fuse
```

#### Mount Filesystem

```bash
./fuse -o allow_other fuse_mount
```

Opsi `allow_other` digunakan agar Docker container dapat mengakses filesystem FUSE.

---

### Pengujian FUSE

#### Membuat File

```bash
echo "halo" > fuse_mount/test.txt
```

#### Hasil pada Storage

```text
encrypted_storage/test.txt.enc
```

Isi file terenkripsi dan tidak dapat dibaca langsung.

#### Membaca File

```bash
cat fuse_mount/test.txt
```

Output:

```text
halo
```

---

### Pengujian File Checker

praktikan membuat direktori:

```bash
mkdir -p encrypted_storage/tests
```

Kemudian menaruh file:

```text
notes.csv.enc
```

pada:

```text
encrypted_storage/tests/
```

Filesystem berhasil melakukan decrypt otomatis ketika file diakses melalui:

```text
fuse_mount/tests/notes.csv
```

---

### Docker Containerization

#### Dockerfile

```dockerfile
FROM ubuntu:latest

WORKDIR /app

COPY server .
COPY client .

RUN chmod +x server
RUN chmod +x client

EXPOSE 9000

CMD ["./server"]
```

Penjelasan:
- menggunakan base image `ubuntu:latest`
- seluruh program diletakkan pada `/app`
- port `9000` diexpose
- server dijalankan otomatis saat container aktif

---

### Build Docker Image

```bash
docker build -t soal-2-modul-4-sisop .
```

---

### Menjalankan Container

```bash
docker run -d \
    --name db_app \
    -p 9000:9000 \
    --mount type=bind,source=$(realpath fuse_mount),target=/app/db \
    soal-2-modul-4-sisop
```

---

### Integrasi Docker dan FUSE

Container menggunakan bind mount:

```text
Host:
fuse_mount
↓
Container:
/app/db
```

Sehingga seluruh perubahan database pada container otomatis diteruskan ke filesystem FUSE.

Alur lengkap:

```text
client
 ↓
server (docker)
 ↓
/app/db
 ↓
fuse_mount
 ↓
FUSE encryption/decryption
 ↓
encrypted_storage
```

---

### Hasil Integrasi

Ketika client menjalankan:

```sql
CREATE DATABASE tes;
```

maka:
- folder database muncul pada `/app/db`
- otomatis muncul pada `fuse_mount`
- otomatis tersimpan pada `encrypted_storage`

Semua file tabel tersimpan dalam bentuk terenkripsi `.enc`.

---

### Kendala

- Mount FUSE Rusak karena root `/` diperlakukan sebagai file `.enc`
- `getattr()` gagal membaca root filesystem
- Docker gagal mount (karena tidak menggunakan `allow_other`

## Soal 3 - LibraryIT

---

### Deskripsi Soal

Pada soal ini, praktikan diminta untuk membangun infrastruktur LibraryIT dari 0 dengan menggunakan Docker dan Samba.

LibraryIT ini memiliki mekanisme:
- Server yang berjalan di dalam kontainer Docker bernama libraryit-server
- Ketika server dijalankan otomatis membuat 4 ruang penyimpanan (`ebooks`, `papers`, `sourcecode`, `docs`) yang berada dalam direktori `/libraryit/`
- Server mengenali 3 user yaitu `member` dengan password `member123`, `contributor` dengan password `contrib456`, dan `librarian` dengan password `lib789`
- Server membagi 3 user menjadi 2 kelompok: readonly (`member`) dan staff (`contributor`, `librarian`)
- Setiap koleksi memiliki aturan akses yang berbeda: `ebooks` dan `paper` dapat dibaca oleh 3 user, namun hanya dapat ditulis oleh `staff`, `sourcecode` tidak boleh diakses sama sekali oleh `member`, `docs` bisa dibaca oleh 3 user, namun hanya dapat ditulis oleh `librarian`
- Seluruh konfigurasi harus berbasis kelompok dan tidak boleh ada akses tanpa identitas
- Seluruh koleksi tersimpan permanen di luar container
- Host hanya dapat mengakses `sourcecode` dengan permission 750
- Host bersifat read-only untuk koleksi `docs`
- Semua aktivitas dicatat pada log

Semua mekanisme harus langsung berjalan otomatis tepat pada saat server berjalan

---

### Struktur Direktori

```bash
soal_3/
├── Dockerfile
├── docker-compose.yml
├── smb.conf
├── entrypoint.sh
├── data/
│   ├── ebooks/
│   ├── papers/
│   ├── sourcecode/
│   └── docs/
└── logs/
    └── libraryit.log
```

---

// Soal belum terselesaikan
