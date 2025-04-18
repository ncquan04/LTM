#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BOARD_MSG_LEN 10
#define MOVE_MSG_LEN   3

// đọc đúng len byte
ssize_t read_exact(int fd, void *buf, size_t len) {
    size_t total = 0;
    char *p = buf;
    while (total < len) {
        ssize_t n = recv(fd, p + total, len - total, 0);
        if (n <= 0) return n;
        total += n;
    }
    return total;
}

void print_board(unsigned char *state) {
    printf("\nCurrent board:\n");
    for (int i = 0; i < 9; ++i) {
        char c = state[i] == 1 ? 'X' : (state[i] == 2 ? 'O' : '.');
        printf("%c ", c);
        if ((i+1)%3 == 0) printf("\n");
    }
    printf("\n");
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in svr = {
        .sin_family = AF_INET,
        .sin_port   = htons(PORT),
        .sin_addr.s_addr = inet_addr("127.0.0.1")
    };

    if (connect(sock, (struct sockaddr*)&svr, sizeof(svr)) < 0) {
        perror("connect");
        return 1;
    }
    printf("Connected to server.\n");

    while (1) {
        // 1) đọc 1 byte loại msg
        unsigned char type;
        if (read_exact(sock, &type, 1) != 1) break;

        if (type == 0x03) {
            // bảng → đọc thêm 9 byte
            unsigned char state[9];
            read_exact(sock, state, 9);
            print_board(state);

        } else if (type == 0x05) {
            // đến lượt → nhập và gửi MOVE_MSG_LEN
            int r, c;
            printf("Your turn! Enter row col (0–2): ");
            scanf("%d %d", &r, &c);
            unsigned char mv[MOVE_MSG_LEN] = {0x02, (unsigned char)r, (unsigned char)c};
            send(sock, mv, MOVE_MSG_LEN, 0);

        } else if (type == 0x04) {
            // kết thúc → đọc thêm 1 byte kết quả
            unsigned char res;
            read_exact(sock, &res, 1);
            if (res == 1)      printf("Player 1 (X) wins!\n");
            else if (res == 2) printf("Player 2 (O) wins!\n");
            else               printf("It's a draw!\n");
            break;

        } else {
            // msg lạ
            fprintf(stderr, "Unknown type: 0x%02x\n", type);
            break;
        }
    }

    close(sock);
    return 0;
}
