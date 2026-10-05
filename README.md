# NetMessenger

## IE3010 - Network Programming Assignment

### Student Personalisation

- Registration Number: IT23634480
- Last Four Digits: 4480
- Server Port: 10480
- Node ID: NID:6344
- Server Source File: server_4480.c
- Client Source File: client_4480.c
- Makefile: Makefile_4480
- Log File: netmsg_IT23634480.log
- Storage Path: ./storage/IT23634480/<sender_username>/<filename>
- Submission Archive: IE3010_IT23634480.zip

---

## Project Overview

NetMessenger is a multi-client TCP/IP chat and file-sharing
application implemented in C using the BSD sockets API.

The system consists of:

- One multi-threaded server
- Multiple TCP clients
- Username registration
- Connected-user listing
- Broadcast messaging
- Private messaging
- Chat rooms
- Room messaging
- File transfer to users and rooms
- Server-side file storage
- Graceful and unexpected disconnect handling
- Error handling
- Server-side event logging

---

## Concurrency Model

The server uses POSIX threads (pthread).

The main server thread accepts incoming TCP connections.
Each connected client is handled by a separate client thread.

Shared data such as the connected-client list and room list
is protected using pthread mutexes to reduce race conditions.

The client also uses a receiver thread so that incoming
messages can be received while the main thread accepts
keyboard commands.

---

## Compilation

Compile the server and client using:

```bash
make -f Makefile_4480
