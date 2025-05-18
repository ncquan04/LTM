#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

int main() {
    fd_set fdset;
    int sockfd1 = socket(AF_INET, SOCK_STREAM, 0);
    int sockfd2 = socket(AF_INET, SOCK_STREAM, 0);
    FD_ZERO(&fdset);
    FD_SET(sockfd1, &fdset);
    FD_SET(sockfd2, &fdset);
    int maxfd = (sockfd1 > sockfd2) ? sockfd1 : sockfd2;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    int res = select(maxfd + 1, &fdset, NULL, NULL, &timeout);
    if (res > 0) {
        printf("%d sockets are ready\n", res);
        for (int i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &fdset)) {
                printf("sockfd%d is set\n", i);
            }
        }
    } else if (res == 0) {
        printf("Timeout\n");
    } else {
        printf("Select error");
    }

}