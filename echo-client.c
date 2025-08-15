#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define BUF_SIZE 512

int sock_fd;
int is_running = 1;

void* recv_thread(void* arg) {
    char buf[BUF_SIZE];
    int recv_len;

    while (is_running) {
        recv_len = recv(sock_fd, buf, sizeof(buf) - 1, 0);
        if (recv_len <= 0) {
            if (is_running) {
                printf("Connection lost\n");
            }
            is_running = 0;
            break;
        }

        buf[recv_len] = '\0';
        printf("Echo: %s\n", buf);
    }

    return NULL;
}

void signal_handler(int sig) {
    printf("\nDisconnecting...\n");
    is_running = 0;
    close(sock_fd);
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }

    char* server_ip = argv[1];
    int port = atoi(argv[2]);

    signal(SIGINT, signal_handler);

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid IP: %s\n", server_ip);
        close(sock_fd);
        return 1;
    }

    printf("Connecting to %s:%d...\n", server_ip, port);
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("Connected! Type messages (Ctrl+C to exit):\n");

    pthread_t tid;
    if (pthread_create(&tid, NULL, recv_thread, NULL) != 0) {
        perror("pthread_create");
        close(sock_fd);
        return 1;
    }

    char msg[BUF_SIZE];
    while (is_running) {
        printf("> ");
        fflush(stdout);

        if (fgets(msg, sizeof(msg), stdin) == NULL) {
            break;
        }

        if (!is_running) break;

        // 개행 문자 제거
        int len = strlen(msg);
        if (len > 0 && msg[len - 1] == '\n') {
            msg[len - 1] = '\0';
        }

        if (strlen(msg) == 0) continue;

        if (send(sock_fd, msg, strlen(msg), 0) == -1) {
            perror("send");
            break;
        }
    }

    is_running = 0;
    close(sock_fd);
    pthread_cancel(tid);

    return 0;
}
