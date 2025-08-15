CC = gcc
CFLAGS = -Wall -pthread
SERVER = echo-server
CLIENT = echo-client

all: $(SERVER) $(CLIENT)

$(SERVER): echo-server.c
	$(CC) $(CFLAGS) -o $(SERVER) echo-server.c

$(CLIENT): echo-client.c
	$(CC) $(CFLAGS) -o $(CLIENT) echo-client.c

clean:
	rm -f $(SERVER) $(CLIENT)

.PHONY: all clean
