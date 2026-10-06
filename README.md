# NetMessenger

IE3010 — Network Programming Assignment

## Student personalisation

- Registration number: IT23634480
- TCP port: 10480 (6000 + 4480)
- Node ID: NID:6344
- Sources: server_4480.c and client_4480.c
- Build file: Makefile_4480
- Event log: netmsg_IT23634480.log
- Server storage: ./storage/IT23634480/<sender_username>/<filename>
- Client downloads: ./received/<filename>

## Overview and requirements

NetMessenger is a multi-client chat and file-sharing application written in C with BSD sockets and POSIX threads. It supports registration, user listing, broadcast and private messages, rooms, file transfers, presence notifications and connection cleanup. Build and run it on Linux with GCC, pthreads and make. The submitted client connects to 127.0.0.1:10480; use separate terminal windows on the same machine for the demonstration.

## Build and run

From the project directory:

```bash
make -f Makefile_4480
```

Start one server:

```bash
./server_4480
```

In each additional terminal:

```bash
cd ~/IE3010_NetMessenger
./client_4480
```

Enter a unique username when prompted. The client sends REGISTER automatically. Keep five clients open for the multi-client demonstration, for example ayesh, nimal, kamal, binura and milinda. Use QUIT to exit a client and Ctrl+C to stop the server. If bind reports that the address is already in use, check for an existing server on port 10480.

## Protocol commands

| Command | Purpose |
| --- | --- |
| REGISTER <username> | Register a unique username; sent by the client at startup. |
| LIST | List connected users. |
| BCAST <message> | Send to all other connected users. |
| PMSG <username> <message> | Send to one connected user. |
| JOIN <room> | Create or join a room. |
| LEAVE <room> | Remove the sender from a room. |
| ROOMS | List available rooms. |
| RMSG <room> <message> | Send to members of a room, including the sender. The sender must be a member. |
| SENDFILE <target> <filename> <filesize> | Send a local file to a username or room. Room senders must be members. Size is in bytes. |
| QUIT | Receive a goodbye response and disconnect. |

OK and ERR responses include NID:6344. Forwarded MSG notifications do not include the NID. Existing users receive MSG JOIN <username> and MSG LEAVE <username> when another user registers or disconnects. Duplicate usernames, missing recipients, malformed commands and non-member room sends are rejected.

## TCP framing and concurrency

TCP is a byte stream. Text commands and response headers end in a newline. A file header is followed by exactly the declared number of raw bytes, with no extra delimiter after the payload. recv_line(), recv_exact() and send_all() handle framing and partial transfers. For a bounded, valid SENDFILE frame, the server consumes the payload before rejecting an unknown target so subsequent commands remain aligned.

The server creates one worker thread per connected client. Mutexes protect shared client, room and log state. Socket write locks keep each forwarded file header and payload together while other threads send messages. The client uses a receiver thread and monitors connection closure while waiting for keyboard input.

## Reproduce a file-integrity test

In a shell terminal:

```bash
printf 'NetMessenger file test
' > transfer_test.txt
wc -c transfer_test.txt
```

In the ayesh client, with nimal connected:

```text
SENDFILE nimal transfer_test.txt 23
```

Back in the shell:

```bash
wc -c transfer_test.txt received/transfer_test.txt storage/IT23634480/ayesh/transfer_test.txt
sha256sum transfer_test.txt received/transfer_test.txt storage/IT23634480/ayesh/transfer_test.txt
ss -ltnp 'sport = :10480'
tail -n 25 netmsg_IT23634480.log
```

All three file sizes should be 23 bytes and all three hashes should match. To test room delivery, both users JOIN labroom before sending to target labroom. A non-member should receive ERR 006 NOT_IN_ROOM.

## Validation and supporting files

The October 6 manual tests verified presence events, graceful and abrupt cleanup, five connected users, broadcast delivery, private/room isolation, LEAVE behaviour, valid user/room file delivery, matching file hashes, unknown-target file rejection, room permission errors, malformed commands, port binding and timestamped logging. TEST_RESULTS.txt separately records 13 automated regression test groups run by the AI assistant on the corrected sources. Those automated results are distinct from the student's Ubuntu screenshots.

README_FIXES.md describes the corrections and limitations. design_diary.md records development decisions; prompt_log.md records AI assistance; reflection.md is a draft for the student to review. The final report and submission packaging still need to be completed according to the current CourseWeb instructions.

## Limits and assumptions

- Maximum 10 clients, 20 rooms and 10 MiB per file.
- Blocking I/O and shared locks can delay other operations if a recipient is slow.
- There is no TLS, password authentication or persistent message history.
- Clients launched in the same directory share the received directory; identical filenames may overwrite an earlier download.
- File names are single protocol tokens. Path separators and unsafe names are rejected.
- A successful sender acknowledgement is not a recipient-issued end-to-end receipt. Check the recipient output and hashes for delivery evidence.
