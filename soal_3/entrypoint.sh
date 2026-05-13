#!/bin/bash

# 1. Pastikan struktur folder tersedia di dalam volume
mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs

# 2. Setup Group & User (Sama seperti sebelumnya)
groupadd staff 2>/dev/null
groupadd readonly 2>/dev/null

useradd -m -G readonly member 2>/dev/null
echo "member:member123" | chpasswd
useradd -m -G staff contributor 2>/dev/null
echo "contributor:contrib456" | chpasswd
useradd -m -G staff librarian 2>/dev/null
echo "librarian:lib789" | chpasswd

# 3. Pendaftaran Samba
(echo "member123"; echo "member123") | smbpasswd -a -s member
(echo "contrib456"; echo "contrib456") | smbpasswd -a -s contributor
(echo "lib789"; echo "lib789") | smbpasswd -a -s librarian

# 4. Keamanan Level Host & Container (Point c)
# Set ownership ke librarian (UID/GID di container harus sinkron dengan host jika ingin rapi)
chown -R librarian:staff /libraryit

# Folder sourcecode hanya boleh diakses pemilik & grup (750)
chmod 750 /libraryit/sourcecode
# Folder lainnya tetap standar perpustakaan
chmod 755 /libraryit/ebooks /libraryit/papers /libraryit/docs

# 5. Jalankan Daemon Samba
exec smbd -F --no-process-group