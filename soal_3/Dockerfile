FROM ubuntu:latest

# Install Samba
RUN apt-get update && apt-get install -y \
    samba \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy konfigurasi dan entrypoint
COPY smb.conf /etc/samba/smb.conf
COPY entrypoint.sh /app/entrypoint.sh

# Beri izin eksekusi pada script inisialisasi
RUN chmod +x /app/entrypoint.sh

# Jalankan entrypoint untuk setup user/folder saat start
ENTRYPOINT ["/app/entrypoint.sh"]