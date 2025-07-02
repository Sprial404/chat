#ifndef CHAT_COMMON_H
#define CHAT_COMMON_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 2 * 1024
#define MAX_USERNAME_SIZE 32
#define MAX_CLIENTS 100
#define MAX_GUILDS 50
#define MAX_CHANNELS_PER_GUILD 50
#define MAX_NAME_LENGTH 64

#define DIE(msg)                                                                                                       \
  do {                                                                                                                 \
    perror(msg);                                                                                                       \
    exit(EXIT_FAILURE);                                                                                                \
  } while (0)

#define PORT 8080

#endif // CHAT_COMMON_H
