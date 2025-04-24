#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 8192

const char* get_mime_type(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext)
        return "text/plain";
    if (strcmp(ext, ".html") == 0)
        return "text/html";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    return "application/octet-stream";
}

void send_404(int client_sock) {
    const char* not_found = 
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    send(client_sock, not_found, strlen(not_found), 0);
}

void handle_get_head(int client_sock, const char* uri, int is_head) {
    char path[1024];
    if (strcmp(uri, "/") == 0) {
        strcpy(path, "index.html");
    } else {
        snprintf(path, sizeof(path), ".%s", uri);
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        send_404(client_sock);
        return;
    }

    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    const char* mime_type = get_mime_type(path);

    char header[512];
    snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n\r\n",
        mime_type, file_size);
    send(client_sock, header, strlen(header), 0);

    if (!is_head) {
        char file_buffer[BUFFER_SIZE];
        int bytes_read;
        while ((bytes_read = read(fd, file_buffer, sizeof(file_buffer))) > 0) {
            send(client_sock, file_buffer, bytes_read, 0);
        }
    }

    close(fd);
}

void handle_post(int client_sock, const char* request, const char* uri) {
    char *body  =strstr(request, "\r\n\r\n");
    if (body) {
        body += 4;
    } else {
        body = "";
    }
    
    char path[1024];
    snprintf(path, sizeof(path), ".%s", uri);

    FILE *fp = fopen(path, "w");
    if (fp) {
        fwrite(body, 1, strlen(body), fp);
        fclose(fp);
    }

    const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    send(client_sock, response, strlen(response), 0);
}

void handle_put(int client_sock, const char* uri, const char* body) {
    char path[1024];
    snprintf(path, sizeof(path), ".%s", uri);
    FILE *fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "%s", body);
        fclose(fp);
        const char* response = 
            "HTTP/1.1 201 Created\r\n"
            "Content-Length: 0\r\n";
        send(client_sock, response, strlen(response), 0);
    } else {
        const char* response = 
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Length: 0\r\n";
        send(client_sock, response, strlen(response), 0);
    }
}

void handle_delete(int client_sock, const char* uri) {
    char path[1024];
    snprintf(path, sizeof(path), ".%s", uri);
    int result = remove(path);
    const char* response;
    if (result == 0) {
        response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n";
    } else {
        response = "HTTP/1.1 404 NOT FOUND\r\nContent-Length: 0\r\n";
    }
    send(client_sock, response, strlen(response), 0);
}

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
        close(client_sock);
        return;
    }
    
    buffer[bytes_received] = '\0';

    char method[8], uri[1024], version[16];
    sscanf(buffer, "%s %s %s", method, uri, version);

    if (strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0) {
        handle_get_head(client_sock, uri, strcmp(method, "HEAD") == 0);
    } else if (strcmp(method, "POST") == 0) {
        handle_post(client_sock, buffer, uri);
    } else if (strcmp(method, "PUT") == 0) {
        char *body = strstr(buffer, "\r\n\r\n");
        if (body) {
            body += 4;
        } else {
            body = "";
        }
        handle_put(client_sock, uri, body);
    } else if (strcmp(method, "DELETE") == 0) {
        handle_delete(client_sock, uri);
    } else {
        const char* response = 
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Content-Length: 0\r\n";
        send(client_sock, response, strlen(response), 0);
    }

    close(client_sock);
    exit(0);
}

void reap_zombie(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main() {
    signal(SIGCHLD, reap_zombie);

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 5);

    printf("Server running on port %d\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_len);
        if (client_sock < 0) {
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            close(server_sock);
            handle_client(client_sock);
        } else {
            close(client_sock);
        }
    }

    close(server_sock);
    return 0;
}