# echo-client-server
echo-client, echo-server 프로그램을 제작하라.

## 참고

https://gitlab.com/gilgil/sns/-/wikis/tcp-and-udp/report-echo-client-server


## Usage

### echo-client
```bash
syntax : echo-client <ip> <port>
sample : ./echo-client 192.168.10.2 1234
````

서버 IP와 포트를 인자로 받아 해당 서버에 접속한다.
사용자가 입력한 문자열을 서버에 전송하고, 서버에서 수신한 데이터를 출력한다.

---

### echo-server

```bash
syntax : echo-server <port> [-e] [-b]
sample : ./echo-server 1234 -e -b
```

서버는 특정 포트에서 클라이언트 접속을 수락한다.

* `-e` 옵션: 받은 메시지를 그대로 에코(echo)하여 클라이언트에게 돌려준다.
* `-b` 옵션: 접속한 모든 클라이언트에게 브로드캐스트한다.


# Vedio 
https://github.com/user-attachments/assets/4181b84f-af94-4e0d-b2ab-c87e2981cbd6

1. **에코 기능 검증**
   서버 실행: `./echo-server 1234 -e`
   → 한 클라이언트에서 전송한 메시지가 같은 클라이언트에만 반환

2. **브로드캐스트 기능 검증**
   서버 실행: `./echo-server 1234 -b`
   → 한 클라이언트가 보낸 메시지가 모든 클라이언트 화면에 표시

3. **기본 접속 테스트**
   서버 실행: `./echo-server 1234`
   클라이언트 실행: `./echo-client 127.0.0.1 1234`

+) **정상 종료 검증** : 각 test마다 클라이언트와 서버에서 인터럽트를 걸어서 리소스가 정상 해제되는지 확인

## echo-client.c 코드 설명

### 소켓 생성 및 서버 연결

```c
sock_fd = socket(AF_INET, SOCK_STREAM, 0);
connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
```

* `socket()`을 통해 TCP 소켓을 생성한다.
* `connect()`를 사용하여 지정된 IP와 포트의 서버에 연결한다.

---

### 수신 전용 스레드

```c
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
```

* 클라이언트는 수신을 별도의 스레드에서 처리한다.
* `recv()`를 통해 서버가 보낸 데이터를 읽어 출력한다.
* 연결이 끊기면 `"Connection lost"` 메시지를 출력한다.

---

### 사용자 입력 처리 및 송신

```c
while (is_running) {
    fgets(msg, sizeof(msg), stdin);
    msg[len - 1] = '\0';   // 개행 문자 제거
    send(sock_fd, msg, strlen(msg), 0);
}
```

* 표준 입력으로 메시지를 받아 `send()`로 서버에 전송한다.
* 개행 문자 제거 후 빈 문자열은 전송하지 않는다.

---

### 시그널 처리

```c
void signal_handler(int sig) {
    is_running = 0;
    close(sock_fd);
    exit(0);
}
```

* `SIGINT`(Ctrl+C)를 받으면 실행 상태를 중단하고 소켓을 닫은 뒤 종료한다.

---

## echo-server.c 코드 설명

### 클라이언트 관리 구조체

```c
struct client_info {
    int sock;
    struct sockaddr_in addr;
    int id;
};
```

* 클라이언트의 소켓, 주소, 식별자(ID)를 관리하는 구조체이다.

---

### 클라이언트 추가 및 제거

```c
void add_client(struct client_info* client) { ... }
void remove_client(int id) { ... }
```

* `add_client()`는 신규 접속을 배열에 추가하고 카운트를 증가시킨다.
* `remove_client()`는 연결 해제된 클라이언트를 정리하고 카운트를 감소시킨다.
* 동시성 문제를 막기 위해 `pthread_mutex`로 보호한다.

---

### 브로드캐스트 함수

```c
void send_to_all(char* msg, int sender_id) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL) {
            if (broadcast_flag || clients[i]->id == sender_id) {
                send(clients[i]->sock, msg, strlen(msg), 0);
            }
        }
    }
```

* `-b` 옵션이 있으면 모든 클라이언트에게 메시지를 보낸다.
* 옵션이 없으면 특정 송신자에게만 메시지를 보낸다.

---

### 클라이언트 처리 스레드

```c
void* handle_client(void* arg) {
    while (recv(client->sock, buf, ...) > 0) {
        printf("Client %d: %s\n", client->id, buf);

        if (echo_flag) {
            if (broadcast_flag) send_to_all(buf, -1);
            else send(client->sock, buf, strlen(buf), 0);
        } else if (broadcast_flag) {
            send_to_all(buf, -1);
        }
    }
    remove_client(client->id);
}
```

* 각 클라이언트마다 스레드가 생성되어 독립적으로 메시지를 처리한다.
* `-e` 옵션이 있으면 송신자 또는 전체에게 에코한다.
* `-b` 옵션이 있으면 전체 브로드캐스트한다.

---

### 서버 메인 루프

```c
server_sock = socket(AF_INET, SOCK_STREAM, 0);
bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
listen(server_sock, 5);

while (1) {
    int client_sock = accept(server_sock, ...);
    pthread_create(&tid, NULL, handle_client, client);
}
```

* 서버는 소켓 생성 후 `bind()`, `listen()`을 통해 포트에 대기한다.
* `accept()`로 새로운 연결을 받아들이고, 클라이언트 처리 스레드를 생성한다.

---

### 서버 종료 처리

```c
void signal_handler(int sig) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]) close(clients[i]->sock);
    }
    close(server_sock);
    exit(0);
}
```

* `SIGINT` 발생 시 모든 클라이언트 소켓과 서버 소켓을 닫고 종료한다.

---

## 옵션 동작 요약

* `-e` : 에코 모드 (송신자에게만 회신)
* `-b` : 브로드캐스트 모드 (모든 클라이언트에게 전송)
* `-e -b` : 모든 클라이언트에게 에코
* 없음 : 서버 콘솔에만 출력

---

## 종료 및 예외 처리

* 클라이언트는 서버 연결이 끊어지면 `"Connection lost"`를 출력하고 입력 루프를 종료한다.
* 클라이언트는 `SIGINT` 수신 시 실행 플래그를 내리고 소켓을 닫은 뒤 즉시 종료한다.
* 서버는 `SIGINT` 수신 시 모든 클라이언트를 정리하고 서버 소켓을 닫은 뒤 종료한다.
* 서버는 `accept()` 오류 발생 시 에러를 출력하고 다음 반복으로 이어간다.
* 서버는 스레드 생성 실패 시 로그를 출력하고 해당 클라이언트를 즉시 제거한다.
