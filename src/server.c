#include "common.h"

struct ClientInfo
{
  int                socket_fd;                   // Socket file descriptor
  struct sockaddr_in address;                     // Client's address
  socklen_t          address_len;                 // Length of the address structure
  char               username[MAX_USERNAME_SIZE]; // Username of the client
  int                current_guild_id;            // Current guild ID
  int                current_channel_id;          // Current channel ID
  int                is_active;                   // Active status, 1 if the slot is in use, 0 if free
};

struct Channel
{
  int  id;
  char name[MAX_NAME_LENGTH];
};

struct Guild
{
  int            id;
  char           name[MAX_NAME_LENGTH];
  int            channel_count;
  struct Channel channels[MAX_CHANNELS_PER_GUILD];
};

static struct ClientInfo clients[MAX_CLIENTS];
static int               client_count = 0;

static struct Guild guilds[MAX_GUILDS];
static int          guild_count = 0;

static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t guild_mutex  = PTHREAD_MUTEX_INITIALIZER;

static void *handle_client(void *arg);
void         send_message_to_client(int client_index, const char *message);
void         broadcast_to_channel(int sender_index, const char *message);
void         parse_and_execute_command(int client_index, const char *command);
int          find_or_create_guild(const char *guild_name);
int          find_or_create_channel(int guild_id, const char *channel_name);

int main(void)
{
  int server_fd;
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    DIE("socket");
  }

  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    DIE("setsockopt");
  }

  struct sockaddr_in server_address;
  server_address.sin_family      = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port        = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
    DIE("bind");
  }

  if (listen(server_fd, MAX_CLIENTS) == -1) {
    DIE("listen");
  }

  printf("Server listening on port %d\n", PORT);

  while (1) {
    int                client_socket;
    struct sockaddr_in client_address;
    socklen_t          client_address_len = sizeof(client_address);

    if ((client_socket = accept(server_fd, (struct sockaddr *)&client_address, &client_address_len)) == -1) {
      perror("accept");
      continue;
    }
    printf("Accepted connection from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

    pthread_mutex_lock(&client_mutex);
    if (client_count >= MAX_CLIENTS) {
      pthread_mutex_unlock(&client_mutex);

      char *error_message = "ERROR Server is full, try again later.\n";
      send(client_socket, error_message, strlen(error_message), 0);
      close(client_socket);

      printf(
          "Max clients reached, rejecting connection from %s:%d\n", inet_ntoa(client_address.sin_addr),
          ntohs(client_address.sin_port)
      );
      continue;
    }

    int client_index = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (!clients[i].is_active) {
        client_index = i;
        break;
      }
    }
    if (client_index == -1) {
      pthread_mutex_unlock(&client_mutex);

      char *error_message = "ERROR Server is full, try again later.\n";
      send(client_socket, error_message, strlen(error_message), 0);
      close(client_socket);

      printf("No free client slot available, rejecting connection\n");
      continue;
    }

    clients[client_index].socket_fd          = client_socket;
    clients[client_index].address            = client_address;
    clients[client_index].address_len        = client_address_len;
    clients[client_index].is_active          = 1;
    clients[client_index].current_guild_id   = -1;
    clients[client_index].current_channel_id = -1;
    strcpy(clients[client_index].username, "Anonymous");
    client_count++;

    int *client_index_ptr = malloc(sizeof(int));
    if (client_index_ptr == NULL) {
      perror("malloc");

      char *error_message = "ERROR An unexpected error occurred, try again later.\n";
      send(client_socket, error_message, strlen(error_message), 0);
      close(client_socket);

      clients[client_index].is_active = 0;
      client_count--;
      pthread_mutex_unlock(&client_mutex);
      continue;
    }
    *client_index_ptr = client_index;

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, handle_client, (void *)client_index_ptr) != 0) {
      perror("pthread_create");

      char *error_message = "ERROR An unexpected error occurred, try again later.\n";
      send(client_socket, error_message, strlen(error_message), 0);
      close(client_socket);

      clients[client_index].is_active = 0;
      client_count--;

      pthread_mutex_unlock(&client_mutex);
      free(client_index_ptr); // Free the allocated memory for client index
      printf("Failed to create thread for client %d, rejecting connection\n", client_index);
      continue;
    }

    pthread_detach(thread_id);
    pthread_mutex_unlock(&client_mutex);
    printf(
        "Client %d (%s:%d) connected and assigned to slot %d\n", client_index, inet_ntoa(client_address.sin_addr),
        ntohs(client_address.sin_port), client_index
    );
  }

  close(server_fd);
  return 0;
}

void *handle_client(void *arg)
{
  int client_index = *(int *)arg;
  free(arg); // Free the allocated memory for client index

  int     client_socket = clients[client_index].socket_fd;
  char    buffer[MAX_BUFFER_SIZE];
  ssize_t bytes_received;

  char welcome_message[100];
  snprintf(welcome_message, sizeof(welcome_message), "INFO Welcome! Please set your username with NAME <username>.\n");
  send(client_socket, welcome_message, strlen(welcome_message), 0);

  while ((bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
    buffer[bytes_received]          = '\0';
    buffer[strcspn(buffer, "\r\n")] = 0; // Remove trailing newline characters

    printf("Received from client %d: %s\n", client_index, buffer);

    parse_and_execute_command(client_index, buffer);
    memset(buffer, 0, sizeof(buffer));
  }

  if (bytes_received == 0) {
    printf(
        "Client %d (%s:%d) disconnected\n", client_index, inet_ntoa(clients[client_index].address.sin_addr),
        ntohs(clients[client_index].address.sin_port)
    );
  } else {
    perror("recv");
  }

  pthread_mutex_lock(&client_mutex);
  clients[client_index].is_active = 0;
  close(client_socket);
  client_count--;
  pthread_mutex_unlock(&client_mutex);

  printf("Client %d disconnected and slot freed\n", client_index);
  return NULL;
}

void parse_and_execute_command(int client_index, const char *buffer)
{
  char *command, *arg1, *arg2, *payload;
  char *saveptr;

  command = strtok_r((char *)buffer, " ", &saveptr);
  if (!command) {
    printf("Client %d sent an empty command\n", client_index);
    return;
  }

  if (strcmp(command, "NAME") == 0) {
    arg1 = strtok_r(NULL, " ", &saveptr);
    if (arg1 && strlen(arg1) < MAX_USERNAME_SIZE) {
      pthread_mutex_lock(&client_mutex);
      int nickname_exists = 0;
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_active && strcmp(clients[i].username, arg1) == 0) {
          nickname_exists = 1;
          break;
        }
      }
      pthread_mutex_unlock(&client_mutex);

      if (nickname_exists) {
        send_message_to_client(client_index, "ERROR Username already taken.\n");
      } else {
        pthread_mutex_lock(&client_mutex);
        strncpy(clients[client_index].username, arg1, MAX_USERNAME_SIZE - 1);
        clients[client_index].username[MAX_USERNAME_SIZE - 1] = '\0';
        pthread_mutex_unlock(&client_mutex);

        char message[MAX_BUFFER_SIZE];
        snprintf(message, sizeof(message), "INFO Username set to %s.\n", clients[client_index].username);
        send_message_to_client(client_index, message);
      }
    } else {
      send_message_to_client(client_index, "ERROR NAME command requires an valid username.\n");
    }
  } else if (strcmp(command, "CREATEGUILD") == 0) {
    arg1 = strtok_r(NULL, " ", &saveptr);
    if (arg1 && strlen(arg1) < MAX_NAME_LENGTH) {
      int guild_id = find_or_create_guild(arg1);
      if (guild_id != -1) {
        char message[MAX_BUFFER_SIZE];
        snprintf(message, sizeof(message), "INFO Guild '%s' created. Default channel '#general' is available.\n", arg1);
        send_message_to_client(client_index, message);
      } else {
        send_message_to_client(client_index, "ERROR Guild limit reached.\n");
      }
    } else {
      send_message_to_client(client_index, "ERROR CREATEGUILD command requires a valid guild name.\n");
    }
  } else if (strcmp(command, "JOIN") == 0) {
    arg1 = strtok_r(NULL, " ", &saveptr); // Guild name
    arg2 = strtok_r(NULL, " ", &saveptr); // Channel name
    if (arg1 && arg2) {
      int guild_id = find_or_create_guild(arg1);
      if (guild_id != -1) {
        int channel_id = find_or_create_channel(guild_id, arg2);
        if (channel_id != -1) {
          pthread_mutex_lock(&client_mutex);
          clients[client_index].current_guild_id   = guild_id;
          clients[client_index].current_channel_id = channel_id;
          pthread_mutex_unlock(&client_mutex);

          char message[MAX_BUFFER_SIZE];
          snprintf(message, sizeof(message), "INFO Joined guild '%s' and channel '%s'.\n", arg1, arg2);
          send_message_to_client(client_index, message);
        } else {
          send_message_to_client(client_index, "ERROR Could not create or join channel, limit reached.\n");
        }
      } else {
        send_message_to_client(client_index, "ERROR Could not create or join guild, limit reached.\n");
      }
    } else {
      send_message_to_client(client_index, "ERROR JOIN command requires a guild name and a channel name.\n");
    }
  } else if (strcmp(command, "MSG") == 0) {
    payload = strtok_r(NULL, "", &saveptr); // Get the rest of the message
    if (payload) {
      // Trim the leading whitespace
      if (*payload == ' ') {
        payload++;
      }

      pthread_mutex_lock(&client_mutex);
      if (clients[client_index].current_guild_id == -1 || clients[client_index].current_channel_id == -1) {
        pthread_mutex_unlock(&client_mutex);
        send_message_to_client(
            client_index,
            "ERROR You must join a guild and channel before sending messages with JOIN <guild> <channel>.\n"
        );
        return;
      }
      pthread_mutex_unlock(&client_mutex);

      char message_to_broadcast[MAX_BUFFER_SIZE];
      snprintf(
          message_to_broadcast, sizeof(message_to_broadcast), "MSG %d %d %s %s\n",
          clients[client_index].current_guild_id, clients[client_index].current_channel_id,
          clients[client_index].username, payload
      );
      broadcast_to_channel(client_index, message_to_broadcast);
    } else {
      send_message_to_client(client_index, "ERROR MSG command requires a message payload.\n");
    }
  } else if (strcmp(command, "LISTGUILDS") == 0) {
    char list_string[MAX_BUFFER_SIZE] = "GUILDLIST ";
    pthread_mutex_lock(&guild_mutex);
    for (int i = 0; i < guild_count; i++) {
      strncat(list_string, guilds[i].name, sizeof(list_string) - strlen(list_string) - 1);
      if (i < guild_count - 1) {
        strncat(list_string, ", ", sizeof(list_string) - strlen(list_string) - 1);
      }
    }
    pthread_mutex_unlock(&guild_mutex);
    strncat(list_string, "\n", sizeof(list_string) - strlen(list_string) - 1);
    send_message_to_client(client_index, list_string);
  } else if (strcmp(command, "LISTCHANNELS") == 0) {
    arg1 = strtok_r(NULL, " ", &saveptr);
    if (arg1) {
      char list_string[MAX_BUFFER_SIZE];
      snprintf(list_string, sizeof(list_string), "CHANNELLIST %s ", arg1);
      pthread_mutex_lock(&guild_mutex);
      int guild_id = -1;
      for (int i = 0; i < guild_count; i++) {
        if (strcmp(guilds[i].name, arg1) == 0) {
          guild_id = guilds[i].id;
          break;
        }
      }
      pthread_mutex_unlock(&guild_mutex);

      if (guild_id != -1) {
        pthread_mutex_lock(&guild_mutex);
        for (int j = 0; j < guilds[guild_id].channel_count; j++) {
          strncat(list_string, guilds[guild_id].channels[j].name, sizeof(list_string) - strlen(list_string) - 1);
          if (j < guilds[guild_id].channel_count - 1) {
            strncat(list_string, ", ", sizeof(list_string) - strlen(list_string) - 1);
          }
        }
        pthread_mutex_unlock(&guild_mutex);

        strncat(list_string, "\n", sizeof(list_string) - strlen(list_string) - 1);
        send_message_to_client(client_index, list_string);
      } else {
        send_message_to_client(client_index, "ERROR Guild not found.\n");
      }
    } else {
      send_message_to_client(client_index, "ERROR LISTCHANNELS command requires a guild name.\n");
    }
  } else if (strcmp(command, "LEAVE") == 0) {
    if (clients[client_index].current_guild_id != -1) {
      pthread_mutex_lock(&client_mutex);
      clients[client_index].current_guild_id   = -1;
      clients[client_index].current_channel_id = -1;
      pthread_mutex_unlock(&client_mutex);

      send_message_to_client(client_index, "INFO Left the current guild and channel.\n");
    } else {
      send_message_to_client(client_index, "ERROR You are not in any guild or channel.\n");
    }
  } else if (strcmp(command, "QUIT") == 0) {
    printf(
        "Client %d (%s:%d) requested to quit\n", client_index, inet_ntoa(clients[client_index].address.sin_addr),
        ntohs(clients[client_index].address.sin_port)
    );
    close(clients[client_index].socket_fd);

    pthread_mutex_lock(&client_mutex);
    clients[client_index].is_active = 0;
    client_count--;
    pthread_mutex_unlock(&client_mutex);

    printf("Client %d disconnected and slot freed\n", client_index);
    pthread_exit(NULL);
  } else {
    send_message_to_client(client_index, "ERROR Unknown command.\n");
  }
}

void send_message_to_client(int client_index, const char *message)
{
  if (client_index < 0 || client_index >= MAX_CLIENTS) {
    fprintf(stderr, "Invalid client index: %d\n", client_index);
    return;
  }

  pthread_mutex_lock(&client_mutex);
  if (clients[client_index].is_active) {
    if (send(clients[client_index].socket_fd, message, strlen(message), 0) == -1) {
      perror("send");
    }
  }
  pthread_mutex_unlock(&client_mutex);
}

void broadcast_to_channel(int sender_index, const char *message)
{
  if (sender_index < 0 || sender_index >= MAX_CLIENTS) {
    fprintf(stderr, "Invalid sender index: %d\n", sender_index);
    return;
  }

  pthread_mutex_lock(&client_mutex);
  int guild_id   = clients[sender_index].current_guild_id;
  int channel_id = clients[sender_index].current_channel_id;

  if (guild_id == -1 || channel_id == -1) {
    pthread_mutex_unlock(&client_mutex);
    return; // Not in a guild or channel
  }

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].is_active && clients[i].current_guild_id == guild_id &&
        clients[i].current_channel_id == channel_id) {
      if (send(clients[i].socket_fd, message, strlen(message), 0) == -1) {
        perror("send");
      }
    }
  }
  pthread_mutex_unlock(&client_mutex);
}

int find_or_create_guild(const char *guild_name)
{
  pthread_mutex_lock(&guild_mutex);
  for (int i = 0; i < guild_count; i++) {
    if (strcmp(guilds[i].name, guild_name) == 0) {
      pthread_mutex_unlock(&guild_mutex);
      return guilds[i].id;
    }
  }

  if (guild_count >= MAX_GUILDS) {
    pthread_mutex_unlock(&guild_mutex);
    return -1; // Guild limit reached
  }

  int new_guild_id = guild_count;
  strncpy(guilds[new_guild_id].name, guild_name, MAX_NAME_LENGTH - 1);
  guilds[new_guild_id].name[MAX_NAME_LENGTH - 1] = '\0';
  guilds[new_guild_id].id                        = new_guild_id;
  guild_count++;
  pthread_mutex_unlock(&guild_mutex);

  // Automatically create a default channel
  find_or_create_channel(new_guild_id, "general");

  return new_guild_id;
}

int find_or_create_channel(int guild_id, const char *channel_name)
{
  pthread_mutex_lock(&guild_mutex);
  if (guild_id < 0 || guild_id >= guild_count) {
    pthread_mutex_unlock(&guild_mutex);
    return -1; // Invalid guild ID
  }

  struct Guild *guild = &guilds[guild_id];
  for (int i = 0; i < guild->channel_count; i++) {
    if (strcmp(guild->channels[i].name, channel_name) == 0) {
      pthread_mutex_unlock(&guild_mutex);
      return guild->channels[i].id; // Channel already exists
    }
  }

  if (guild->channel_count >= MAX_CHANNELS_PER_GUILD) {
    pthread_mutex_unlock(&guild_mutex);
    return -1; // Channel limit reached
  }

  int new_channel_id = guild->channel_count;
  strncpy(guild->channels[new_channel_id].name, channel_name, MAX_NAME_LENGTH - 1);
  guild->channels[new_channel_id].name[MAX_NAME_LENGTH - 1] = '\0';
  guild->channels[new_channel_id].id                        = new_channel_id;
  guild->channel_count++;

  pthread_mutex_unlock(&guild_mutex);
  return new_channel_id;
}
