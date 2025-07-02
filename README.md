# Chat Application

A multi-threaded client-server chat application written in C that supports multiple guilds and channels, similar to Discord's architecture.

## Features

- **Multi-threaded server**: Handles multiple clients concurrently using pthreads
- **Guild and channel system**: Organize conversations into guilds with multiple channels
- **Real-time messaging**: Live chat with instant message delivery
- **Thread-safe**: Proper synchronization using mutexes
- **Cross-platform**: Compatible with Unix-like systems (Linux, macOS)

## Building

### Prerequisites

- C compiler
- POSIX-compliant system (Linux, macOS, etc.)
- pthread library

### Compilation

```bash
$ make
```

This will create two executables in the `out/` directory:
- `out/chat_server` - The chat server
- `out/chat_client` - The chat client

### Cleaning

```bash
$ make clean
```

## Usage

### Starting the Server

```bash
$ ./out/chat_server
```

The server will start listening on port 8080 by default.

### Connecting Clients

```bash
$ ./out/chat_client <server_ip> <server_port>
```

## Configuration

The application uses several configurable constants in `include/common.h`:

- `PORT`: Server port (default: 8080)
- `MAX_CLIENTS`: Maximum concurrent clients
- `MAX_GUILDS`: Maximum number of guilds
- `MAX_CHANNELS_PER_GUILD`: Maximum channels per guild
- `BUFFER_SIZE`: Message buffer size

## Network Protocol

The application uses a simple text-based protocol over TCP:

- `MSG <guild> <channel> <username> <message>` - Send a message
- `INFO <message>` - Server information/status messages

## Limitations

- No message persistence (messages are not saved to disk)
- No user authentication or authorization
- No message history for new clients
- Fixed maximum limits for clients, guilds, and channels

## Future Enhancements

- [ ] Message persistence
- [ ] User authentication and permissions
- [ ] Private messaging
- [ ] Message encryption
- [ ] User presence indicators

## License

This project is available under the MIT License.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## Troubleshooting

### Common Issues

**Server won't start**: Check if port 8080 is already in use:
```bash
$ lsof -i :8080
```

**Client can't connect**: Ensure the server is running and firewall allows connections on port 8080.

**Compilation errors**: Make sure you have GCC and pthread library installed:
```bash
# On Ubuntu/Debian
$ sudo apt-get install build-essential

# On macOS (with Homebrew)
$ brew install gcc
```
