#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[2048] = {0};
    char command[1024] = {0};

    printf("Connected to DB Server on port %d\n", PORT);
    printf("Type HELP for available commands\n");
    printf("Type EXIT to quit\n\n");

    while (1) {
        printf("db > ");
        if (fgets(command, sizeof(command), stdin) == NULL) break;

        // Buang karakter newline (\n) di akhir input
        command[strcspn(command, "\n")] = 0;

        if (strcasecmp(command, "EXIT") == 0) break;
        if (strlen(command) == 0) continue;

        // Buat socket baru untuk setiap perintah
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            printf("\n Socket creation error \n");
            return -1;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("Connection Failed. Pastikan container Docker sudah jalan!\n");
            close(sock);
            continue;
        }

        // Kirim perintah ke server
        send(sock, command, strlen(command), 0);

        // Baca balasan dari server
        memset(buffer, 0, sizeof(buffer));
        int valread = read(sock, buffer, sizeof(buffer));
        
        if (valread > 0) {
            printf("%s\n", buffer);
        } else {
            printf("(Tidak ada balasan dari server)\n");
        }

        close(sock);
    }

    return 0;
}