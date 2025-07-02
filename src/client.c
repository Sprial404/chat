#include "common.h"

static int socket_fd;
char       name[MAX_USERNAME_SIZE];

static void *receive_messages(void *arg);
static void  print_prompt(void);
static int   send_message(const char *message);

int main(int argc, char *argv[])
{
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <server_ip> <name>\n", argv[0]);
    return 1;
  }

  const char *server_ip = argv[1];
  strncpy(name, argv[2], MAX_USERNAME_SIZE - 1);
  name[MAX_USERNAME_SIZE - 1] = '\0';

  if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    DIE("Socket creation failed");
  }

  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_port   = htons(PORT);

  if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) <= 0) {
    DIE("Invalid address or address not supported");
  }

  if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
    DIE("Connection failed");
  }

  char buffer[MAX_BUFFER_SIZE];
  snprintf(buffer, sizeof(buffer), "NAME %s", name);
  if (send(socket_fd, buffer, strlen(buffer), 0) == -1) {
    perror("Failed to send name");
    close(socket_fd);
    return 1;
  }

  pthread_t receive_thread;
  if (pthread_create(&receive_thread, NULL, receive_messages, (void *)&socket_fd) != 0) {
    DIE("Failed to create receive thread");
  }

  printf("Connected to server. You can start sending messages.\nType '/help' for available commands.\n");

  while (1) {
    print_prompt();
    if (fgets(buffer, sizeof(buffer) - 1, stdin) == NULL) {
      perror("fgets");
      continue;
    }
    buffer[strcspn(buffer, "\n\r")] = 0; // Remove trailing newline characters

    if (strlen(buffer) == 0) {
      continue; // Skip empty input
    }

    char send_buffer[MAX_BUFFER_SIZE];

    if (buffer[0] == '/') {
      char *command, *arg1, *arg2;
      char *saveptr;

      char temp_buffer[MAX_BUFFER_SIZE];
      strncpy(temp_buffer, buffer + 1, sizeof(temp_buffer) - 1);
      temp_buffer[sizeof(temp_buffer) - 1] = '\0';

      command = strtok_r(temp_buffer, " ", &saveptr);
      if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
        snprintf(send_buffer, sizeof(send_buffer), "QUIT\n");
        send_message(send_buffer);
        break;
      } else if (strcmp(command, "join") == 0) {
        arg1 = strtok_r(NULL, " ", &saveptr); // Guild name
        arg2 = strtok_r(NULL, " ", &saveptr); // Channel name
        if (arg1 && arg2) {
          snprintf(send_buffer, sizeof(send_buffer), "JOIN %s %s", arg1, arg2);
        } else {
          fprintf(stderr, "Usage: /join <guild> <channel>\n");
          continue;
        }
      } else if (strcmp(command, "leave") == 0) {
        snprintf(send_buffer, sizeof(send_buffer), "LEAVE\n");
      } else if (strcmp(command, "createguild") == 0) {
        arg1 = strtok_r(NULL, " ", &saveptr);
        if (arg1) {
          snprintf(send_buffer, sizeof(send_buffer), "CREATEGUILD %s", arg1);
        } else {
          fprintf(stderr, "Usage: /createguild <guild_name>\n");
          continue;
        }
      } else if (strcmp(command, "listguilds") == 0) {
        snprintf(send_buffer, sizeof(send_buffer), "LISTGUILDS\n");
      } else if (strcmp(command, "listchannels") == 0) {
        arg1 = strtok_r(NULL, " ", &saveptr);
        if (arg1) {
          snprintf(send_buffer, sizeof(send_buffer), "LISTCHANNELS %s", arg1);
        } else {
          fprintf(stderr, "Usage: /listchannels <guild>\n");
        }
      } else if (strcmp(command, "help") == 0) {
        printf(
            "Available commands:\n"
            "\t/join <guild> <channel> - Join a guild and channel\n"
            "\t/leave - Leave the current guild and channel\n"
            "\t/listguilds - List all guilds\n"
            "\t/listchannels <guild> - List channels in a guild\n"
            "\t/quit or /exit - Exit the client\n"
        );
        continue;
      } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        continue;
      }
    } else {
      // Regular message
      snprintf(send_buffer, sizeof(send_buffer), "MSG %s", buffer);
    }

    if (send_message(send_buffer) == -1) {
      fprintf(stderr, "Failed to send message: %s\n", send_buffer);
      break;
    }

    memset(send_buffer, 0, sizeof(send_buffer)); // Clear the send buffer
  }

  close(socket_fd);
  pthread_cancel(receive_thread);
  pthread_join(receive_thread, NULL);
  return 0;
}

static void print_prompt(void)
{
  printf("%s> ", name);
  fflush(stdout);
}

static int send_message(const char *message)
{
  if (send(socket_fd, message, strlen(message), 0) == -1) {
    perror("Failed to send message");
    return -1;
  }
  return 0;
}

static void *receive_messages(void *arg)
{
  int     socket_fd = *(int *)arg;
  char    server_reply[MAX_BUFFER_SIZE];
  ssize_t bytes_received;

  while ((bytes_received = recv(socket_fd, server_reply, sizeof(server_reply) - 1, 0)) > 0) {
    server_reply[bytes_received] = '\0';

    // Clear the terminal line
    printf("\033[K\r");
    fflush(stdout);

    char *command, *arg1, *arg2, *arg3, *payload;
    char *saveptr;

    char temp_reply[MAX_BUFFER_SIZE];
    strncpy(temp_reply, server_reply, sizeof(temp_reply) - 1);
    temp_reply[sizeof(temp_reply) - 1]      = '\0';
    temp_reply[strcspn(temp_reply, "\r\n")] = 0; // Remove trailing newline characters

    command = strtok_r(temp_reply, " ", &saveptr);
    if (command == NULL) {
      continue; // Skip empty messages
    }

    if (strcmp(command, "MSG") == 0) {
      arg1    = strtok_r(NULL, " ", &saveptr); // Guild ID
      arg2    = strtok_r(NULL, " ", &saveptr); // Channel ID
      arg3    = strtok_r(NULL, " ", &saveptr); // Username
      payload = strtok_r(NULL, "", &saveptr);  // Message payload

      if (arg1 && arg2 && arg3 && payload) {
        if (*payload == ' ') {
          payload++; // Trim leading whitespace
        }

        printf("[%s/%s] <%s>: %s\n", arg1, arg2, arg3, payload);
      } else {
        fprintf(stderr, "Malformed message received: %s\n", server_reply);
      }
    } else if (strcmp(command, "INFO") == 0) {
      payload = strtok_r(NULL, "", &saveptr); // Get the rest of the message
      if (payload) {
        if (*payload == ' ') {
          payload++; // Trim leading whitespace
        }
        printf("[Server INFO]: %s\n", payload);
      }
    } else if (strcmp(command, "ERROR") == 0) {
      payload = strtok_r(NULL, "", &saveptr); // Get the rest of the message
      if (payload) {
        if (*payload == ' ') {
          payload++; // Trim leading whitespace
        }

        fprintf(stderr, "[Server ERROR]: %s\n", payload);
      }
    } else if (strcmp(command, "GUILDLIST") == 0) {
      payload = strtok_r(NULL, "", &saveptr); // Get the rest of the message
      if (payload) {
        if (*payload == ' ') {
          payload++; // Trim leading whitespace
        }

        printf("[Guilds]: %s\n", payload);
      } else {
        printf("[Guilds]: No guilds available.\n");
      }
    } else if (strcmp(command, "CHANNELLIST") == 0) {
      arg1    = strtok_r(NULL, " ", &saveptr); // Guild name
      payload = strtok_r(NULL, "", &saveptr);  // Get the rest of the message
      if (arg1) {
        if (payload && *payload == ' ') {
          payload++; // Trim leading whitespace
        }
        printf("[Channels in %s]: %s\n", arg1, payload ? payload : "No channels available.");
      }
    } else {
      // Unknown command, print the raw server reply
      fprintf(stderr, "%s", server_reply);
    }

    print_prompt(); // Print the prompt again after processing the message
  }

  if (bytes_received == 0) {
    printf("\r\033[K[Server disconnected]\n");
    fflush(stdout);

    exit(0); // Exit the client gracefully
  } else if (bytes_received == -1) {
    perror("recv");
  }

  return NULL; // Thread exit
}
