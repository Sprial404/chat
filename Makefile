CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -Iinclude -g
LDFLAGS = -lpthread

SERVER_TARGET = out/chat_server
SERVER_SRCS = src/server.c
SERVER_OBJS = $(patsubst src/%.c, out/%.o, $(SERVER_SRCS))

CLIENT_TARGET = out/chat_client
CLIENT_SRCS = src/client.c
CLIENT_OBJS = $(patsubst src/%.c, out/%.o, $(CLIENT_SRCS))

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_OBJS)
	@mkdir -p $(@D)
	$(CC) -o $@ $^ $(LDFLAGS)

$(CLIENT_TARGET): $(CLIENT_OBJS)
	@mkdir -p $(@D)
	$(CC) -o $@ $^ $(LDFLAGS)

out/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf out

.PHONY: all clean