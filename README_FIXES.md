# NetMessenger corrections — IT23634480

This package updates the supplied server_4480.c and client_4480.c. The TCP port remains 10480, the NID remains 6344, and the project uses C BSD sockets and a thread per client.

## Install and run

1. Keep a backup of your existing source files.
2. Copy server_4480.c, client_4480.c and Makefile_4480 into ~/IE3010_NetMessenger.
3. Stop the currently running server and clients before running the new build. Compiling alone does not update an already running process.
4. Build: `make -f Makefile_4480 -B`
5. Start the server: `./server_4480`
6. In separate terminals, start clients: `./client_4480`

## Corrections

- Successful registration notifies existing clients with `MSG JOIN <username>`.
- QUIT and unexpected disconnect remove the user and room memberships, then notify the remaining clients with `MSG LEAVE <username>`.
- RMSG checks the sender's membership under the room mutex; non-members receive `ERR 006 NOT_IN_ROOM NID:6344`.
- Room file transfers also require sender membership.
- A valid, bounded SENDFILE header is followed by exactly the declared payload bytes. The server consumes these bytes before rejecting a missing target or invalid filename. The next command remains aligned, including when the rejected payload contains no newline.
- Malformed file headers, invalid/oversized sizes and allocation failures close the connection because safe framing cannot be preserved. No extra READY handshake or payload newline is added.
- Writes to each socket are serialized. A file header and its payload share one lock so other messages cannot appear inside the payload.
- Broken-pipe sends do not terminate the process; interrupted socket operations are retried.
- The broadcast buffer now accommodates every accepted command line. Oversized/incomplete frames are not processed as valid commands.
- Usernames are bounded safe directory components; extra registration tokens and path traversal usernames are rejected.
- Incoming filenames are checked before saving. Disk write failures are reported instead of printing success.
- The client rejects oversized keyboard commands and exits when the server closes the connection. Its receiver thread is joined before the socket is closed.
- Malformed command and network presence events are logged with timestamps.

## Validation

Both sources compiled with `gcc -Wall -Wextra -Werror -pthread`. The server was tested with AddressSanitizer and UndefinedBehaviorSanitizer; no sanitizer errors were reported. TEST_RESULTS.txt lists the 13 passing regression groups. Tests included five clients, binary and empty files, split/coalesced TCP input, a rejected payload without a newline, a 950-character broadcast, two concurrent 128 KiB files plus twenty private messages, and the real C client's send/receive, QUIT and server-disconnect behavior.

Repeat the relevant tests on your Ubuntu machine after installation and capture your own screenshots for the report. Existing screenshots of failed behavior should be described as pre-fix evidence.

## Implementation limits

The maximum file size remains 10 MiB. The existing blocking-socket architecture is retained: a slow receiver can delay a send and operations holding the client/room mutex. Send locks use 64 stripes, so unrelated sockets can occasionally share a lock. TLS, authentication and persistent message history are not added. The client connects to 127.0.0.1, and clients started from the same directory share the received directory. These local regression tests do not replace cross-machine network tests.
