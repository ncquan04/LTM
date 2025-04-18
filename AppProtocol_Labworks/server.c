#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BOARD_MSG_LEN 10   // 1 byte type + 9 bytes state
#define MOVE_MSG_LEN   3   // 1 byte type + 2 bytes coord

// đọc đúng len byte từ fd vào buf, trả về tổng số byte đã đọc (<=0 nếu lỗi)
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

// gửi đúng len byte từ buf tới fd, trả về tổng số byte đã gửi (<=0 nếu lỗi)
ssize_t send_all(int fd, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = buf;
    while (total < len) {
        ssize_t n = send(fd, p + total, len - total, 0);
        if (n <= 0) return n;
        total += n;
    }
    return total;
}

int board[3][3] = {0};

// gửi bảng trạng thái tới cả hai client
void broadcast_board(int p1, int p2) {
    unsigned char msg[BOARD_MSG_LEN];
    msg[0] = 0x03;
    int k = 1;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            msg[k++] = board[i][j];
    send_all(p1, msg, BOARD_MSG_LEN);
    send_all(p2, msg, BOARD_MSG_LEN);
}

// gửi chỉ thị tới lượt cho một client
void send_turn(int p) {
    unsigned char t = 0x05;
    send_all(p, &t, 1);
}

// kiểm tra thắng, trả về 1 hoặc 2 nếu có người thắng, 0 nếu chưa, 3 nếu hòa
int check_winner() {
    // hàng và cột
    for (int i = 0; i < 3; ++i) {
        if (board[i][0] && board[i][0] == board[i][1] && board[i][1] == board[i][2])
            return board[i][0];
        if (board[0][i] && board[0][i] == board[1][i] && board[1][i] == board[2][i])
            return board[0][i];
    }
    // 2 đường chéo
    if (board[0][0] && board[0][0] == board[1][1] && board[1][1] == board[2][2])
        return board[0][0];
    if (board[0][2] && board[0][2] == board[1][1] && board[1][1] == board[2][0])
        return board[0][2];
    return 0;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 2);

    printf("Waiting for players...\n");
    int p1 = accept(server_fd, (struct sockaddr*)&addr, &addrlen);
    printf("Player 1 connected.\n");
    int p2 = accept(server_fd, (struct sockaddr*)&addr, &addrlen);
    printf("Player 2 connected.\n");

    int current = 1;    // 1 = p1, 2 = p2
    int move_count = 0;

    while (1) {
        int active = (current == 1) ? p1 : p2;

        // 1) thông báo lượt
        send_turn(active);

        // 2) chờ đúng MOVE_MSG_LEN từ player đúng lượt
        unsigned char buf[MOVE_MSG_LEN];
        while (1) {
            if (read_exact(active, buf, MOVE_MSG_LEN) != MOVE_MSG_LEN) {
                perror("recv move");
                exit(1);
            }
            if (buf[0] != 0x02) {
                // sai định dạng → gửi lại lượt
                send_turn(active);
                continue;
            }
            int r = buf[1], c = buf[2];
            if (r<0||r>2||c<0||c>2 || board[r][c]!=0) {
                // ô sai → gửi lại lượt
                send_turn(active);
                continue;
            }
            // hợp lệ
            board[r][c] = current;
            move_count++;
            break;
        }

        // 3) gửi lại bảng cho cả 2
        broadcast_board(p1, p2);

        // 4) kiểm thắng/hòa
        int w = check_winner();
        if (w || move_count == 9) {
            unsigned char end_msg[2] = {0x04, (unsigned char)(w ? w : 3)};
            send_all(p1, end_msg, 2);
            send_all(p2, end_msg, 2);
            break;
        }

        // 5) chuyển lượt
        current = 3 - current;
    }

    close(p1);
    close(p2);
    close(server_fd);
    printf("Game over.\n");
    return 0;
}
