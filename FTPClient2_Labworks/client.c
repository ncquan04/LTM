#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

void read_response(int sock) {
    char buffer[BUFFER_SIZE];
    int len = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (len > 0) {
        buffer[len] = '\0';
        printf("Response: %s", buffer);
    }
}

int open_data_connection(int control_sock) {
    char buffer[BUFFER_SIZE];
    send(control_sock, "PASV\r\n", 6, 0);
    recv(control_sock, buffer, sizeof(buffer), 0);
    printf("PASV %s", buffer);

    int h1, h2, h3, h4, p1, p2;
    sscanf(strrchr(buffer, '('), "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2);
    int port = p1 * 256 + p2;
    char ip[64];
    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);

    int data_sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in data_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    inet_pton(AF_INET, ip, &data_addr.sin_addr);

    if (connect(data_sock, (struct sockaddr*) &data_addr, sizeof(data_addr)) < 0) {
        perror("Connect data socket");
        exit(1);
    }

    return data_sock;
}

void ftp_retr(int control_sock, const char* filename) {
    int data_sock = open_data_connection(control_sock);

    char cmd[256];
    sprintf(cmd, "RETR %s\r\n", filename);
    send(control_sock, cmd, strlen(cmd), 0);
    read_response(control_sock);

    FILE* fp = fopen(filename, "wb");
    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes) = recv(data_sock, buffer, sizeof(buffer), 0) > 0) {
        fwrite(buffer, 1, bytes, fp);
    }

    fclose(fp);
    close(data_sock);
    read_response(control_sock);
}

void ftp_stor(int control_sock, const char *filename) {
    int data_sock = open_data_connection(control_sock);

    char cmd[256];
    sprintf(cmd, "STROR %s\r\n", filename);
    send(control_sock, cmd, strlen(cmd), 0);
    read_response(control_sock);

    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        close(data_sock);
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        send(data_sock, buffer, bytes, 0);
    }

    fclose(fp);
    close(data_sock);
    read_response(control_sock);
}

void ftp_delete(int control_sock, const char* filename) {
    char cmd[256];
    sprintf(cmd, "DELE %s\r\n", filename);
    send(control_sock, cmd, strlen(cmd), 0);
    read_response(control_sock);
}

void ftp_cwd(int control_sock, const char* dirname) {
    char cmd[256];
    sprintf(cmd, "CWD %s\r\n", dirname);
    send(control_sock, cmd, strlen(cmd), 0);
    read_response(control_sock);
}

void ftp_list(int control_sock) {
    int data_sock = open_data_connection(control_sock);
    
    send(control_sock, "LIST\r\n", 6, 0);
    read_response(control_sock);

    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = recv(data_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }

    close(data_sock);
    read_response(control_sock);
}

int main() {
    int control_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(21),
    };
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(control_sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        perror("Connect control socket");
        return 1;
    }

    read_response(control_sock);

    send(control_sock, "USER user\r\n", strlen("USER user\r\n"), 0);
    read_response(control_sock);

    send(control_sock, "PASS pass\r\n", strlen("PASS pass\r\n"), 0);
    read_response(control_sock);

    char cmd[32], arg[256];
    while (1) {
        printf("\nChon chuc nang:\n");
        printf("1. RETR (tai file tu server)\n");
        printf("2. STOR (gui file len server)\n");
        printf("3. DELE (xoa file tren server)\n");
        printf("4. CWD  (doi thu muc tren server)\n");
        printf("5. LIST (liet ke file tren server)\n");
        printf("6. QUIT (thoat)\n");
        printf("Nhap lua chon: ");
        fgets(cmd, sizeof(cmd), stdin);

        int choice = atoi(cmd);
        switch (choice) {
            case 1:
                printf("Nhap ten file can tai: ");
                fgets(arg, sizeof(arg), stdin);
                arg[strcspn(arg, "\r\n")] = 0;
                ftp_retr(control_sock, arg);
                break;
            case 2:
                printf("Nhap ten file can gui: ");
                fgets(arg, sizeof(arg), stdin);
                arg[strcspn(arg, "\r\n")] = 0;
                ftp_stor(control_sock, arg);
                break;
            case 3:
                printf("Nhap ten file can xoa: ");
                fgets(arg, sizeof(arg), stdin);
                arg[strcspn(arg, "\r\n")] = 0;
                ftp_delete(control_sock, arg);
                break;
            case 4:
                printf("Nhap ten thu muc can chuyen: ");
                fgets(arg, sizeof(arg), stdin);
                arg[strcspn(arg, "\r\n")] = 0;
                ftp_cwd(control_sock, arg);
                break;
            case 5:
                ftp_list(control_sock);
                break;
            case 6:
                send(control_sock, "QUIT\r\n", strlen("QUIT\r\n"), 0);
                read_response(control_sock);
                close(control_sock);
                return 0;
            default:
                printf("Lua chon khong hop le!\n");
        }
    }
}