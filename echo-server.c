#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define MAX_CLIENTS 50
#define BUF_SIZE 512

struct client_info {
    int sock;
    struct sockaddr_in addr;
    int id;
};

struct client_info* clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int echo_flag = 0;
int broadcast_flag = 0;
int server_sock;

void add_client(struct client_info* client) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            clients[i] = client;
            client_count++;
            printf("Client %d connected [%s:%d] - Total: %d\n",
                   client->id, inet_ntoa(client->addr.sin_addr),
                   ntohs(client->addr.sin_port), client_count);
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int id) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->id == id) {
            printf("Client %d disconnected - Total: %d\n", id, client_count - 1);
            close(clients[i]->sock);
            free(clients[i]);
            clients[i] = NULL;
            client_count--;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_to_all(char* msg, int sender_id) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL) {
            if (broadcast_flag || clients[i]->id == sender_id) {
                send(clients[i]->sock, msg, strlen(msg), 0);
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void* handle_client(void* arg) {
    struct client_info* client = (struct client_info*)arg;
    char buf[BUF_SIZE];
    int recv_len;

    while ((recv_len = recv(client->sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[recv_len] = '\0';

        // 개행 문자 제거
        if (buf[recv_len - 1] == '\n') {
            buf[recv_len - 1] = '\0';
        }

        printf("Client %d: %s\n", client->id, buf);

        if (echo_flag) {
            if (broadcast_flag) {
                send_to_all(buf, -1);  // 모든 클라이언트에게
            } else {
                send(client->sock, buf, strlen(buf), 0);  // 송신자에게만
            }
        } else if (broadcast_flag) {
            send_to_all(buf, -1);  // 브로드캐스트만
        }
    }

    remove_client(client->id);
    return NULL;
}

void signal_handler(int sig) {
    printf("\nServer shutting down...\n");

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]) {
            close(clients[i]->sock);
            free(clients[i]);
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(server_sock);
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <port> [-e] [-b]\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    // 옵션 파싱
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-e") == 0) {
            echo_flag = 1;
        } else if (strcmp(argv[i], "-b") == 0) {
            broadcast_flag = 1;
        }
    }

    signal(SIGINT, signal_handler);

    // 소켓 생성
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 5) == -1) {
        perror("listen");
        close(server_sock);
        return 1;
    }

    printf("Server listening on port %d\n", port);
    printf("Echo: %s, Broadcast: %s\n",
           echo_flag ? "ON" : "OFF", broadcast_flag ? "ON" : "OFF");

    int client_id = 1;

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_len);
        if (client_sock == -1) {
            perror("accept");
            continue;
        }

        struct client_info* client = malloc(sizeof(struct client_info));
        client->sock = client_sock;
        client->addr = client_addr;
        client->id = client_id++;

        add_client(client);

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client) != 0) {
            perror("pthread_create");
            remove_client(client->id);
            continue;
        }
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}
