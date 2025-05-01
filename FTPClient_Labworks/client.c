#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

void send_command(int sockfd, const char* cmd) {
    write(sockfd, cmd, strlen(cmd));
}

int receive_response(int sockfd, char* buffer, int size) {
    int n = read(sockfd, buffer, size - 1);
    buffer[n] = '\0';
    printf("%s", buffer);
    return n;
}

void extract_pasv_ip_port(char* response, char* ip, int* port) {
    int h1, h2, h3, h4, p1, p2;
    sscanf(response, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2);
    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    *port = p1 * 256 + p2;
}

int create_data_connection(const char* ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Data connection failed");
        return -1;
    }
    return sock;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[1024];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(21);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    receive_response(sockfd, buffer, sizeof(buffer));

    send_command(sockfd, "USER user\r\n");
    receive_response(sockfd, buffer, sizeof(buffer));

    send_command(sockfd, "PASS pass\r\n");
    receive_response(sockfd, buffer, sizeof(buffer));

    send_command(sockfd, "PASV\r\n");
    receive_response(sockfd, buffer, sizeof(buffer));

    char data_ip[32];
    int data_port;
    extract_pasv_ip_port(buffer, data_ip, &data_port);

    int data_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in data_addr;
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(data_port);
    inet_pton(AF_INET, data_ip, &data_addr.sin_addr);
    if (connect(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        perror("Data connection failed");
        exit(EXIT_FAILURE);
    }

    send_command(sockfd, "LIST\r\n");
    receive_response(sockfd, buffer, sizeof(buffer));

    int n;
    while ((n = read(data_sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        printf("%s", buffer);
    }

    close(data_sock);
    receive_response(sockfd, buffer, sizeof(buffer));

    send_command(sockfd, "PASV\r\n");
    receive_response(sockfd, buffer, sizeof(buffer));
    extract_pasv_ip_port(buffer, data_ip, &data_port);
    data_sock = create_data_connection(data_ip, data_port);
    if (data_sock < 0) {
        perror("Data connection failed");
        exit(EXIT_FAILURE);
    }

    const char* filename = "test.txt";
    char retr_cmd[256];
    snprintf(retr_cmd, sizeof(retr_cmd), "RETR %s\r\n", filename);
    send_command(sockfd, retr_cmd);
    receive_response(sockfd, buffer, sizeof(buffer));

    FILE* out_file = fopen("downloaded_test.txt", "wb");
    if (!out_file) {
        perror("Cannot open output file");
        close(data_sock);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    while ((n = read(data_sock, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, n, out_file);
    }
    fclose(out_file);
    close(data_sock);
    receive_response(sockfd, buffer, sizeof(buffer));

    printf("File downloaded successfully.\n");

    close(sockfd);
    return 0;
}