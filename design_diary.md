# NetMessenger Design Diary

Registration number: IT23634480

## Planning and architecture

I identified the mandatory commands and calculated port 10480 and NID:6344 from my registration number. I selected a POSIX thread-per-client server so the accept loop could continue while workers processed clients. Shared user, room and log data require mutex protection. The client has a receiver thread so incoming messages can arrive while I enter commands.

## Development sequence

I developed and tested basic socket communication, registration and duplicate-name rejection, LIST, broadcast/private messaging, rooms, file transfer, disconnect cleanup and logging. The server uses socket(), bind(), listen() and accept(); clients use socket() and connect(). Tests used 127.0.0.1:10480. Git records the actual implementation sequence; I have not assigned retrospective dates to these earlier stages here.

TCP framing was a central design issue. Text lines end in a newline, while file headers are followed by an exact raw-byte payload. recv_line(), recv_exact() and send_all() handle the stream. The server stores files under ./storage/IT23634480/<sender_username>/<filename>. I compared original, stored and received copies using SHA-256.

## October 6, 2026 — Review, corrections and retesting

Manual testing exposed missing join/leave notifications, acceptance of room messages from non-members, and an extra invalid-command error after sending a file to an unknown user. I uploaded my sources to ChatGPT/Codex. The assistant produced corrected C files and ran 13 automated regression test groups, including concurrent file/message framing. I then installed and compiled the corrected files in Ubuntu and retested their behaviour myself.

The fixes add presence notifications and room sender membership checks. The server consumes valid bounded file payloads before reporting target errors. Socket write locks keep file headers and payloads together. Additional changes validate input and file paths and improve client disconnect handling.

My Ubuntu retests confirmed clean compilation, JOIN/LEAVE notifications, QUIT and Ctrl+C cleanup, five connected users, broadcast delivery, private/room isolation, LEAVE behaviour, user/room file delivery, matching hashes for the 23-byte test file, unknown-target rejection and malformed-command responses. I checked port 10480 with ss and timestamped events in the log. I committed and pushed the corrected sources and supporting results as ab309c8.

## Remaining work and limitations

I still need to complete the report and submission package and prepare to explain the code in the viva. Blocking sends can delay other operations; TLS, authentication and persistent history are not implemented.

### 6 October 2026 — Optional Chat Rate Limiting

Added per-client rate limiting on the feature/rate-limiting branch
after preserving the tested core implementation in Git.

Each connection can submit 10 BCAST, PMSG or RMSG attempts within
a 5-second fixed window. The counter and window start time are
local to the client handler thread, so clients have independent
limits. CLOCK_MONOTONIC measures elapsed time. Excess attempts
receive ERR 011 RATE_LIMITED without closing the connection.

AI assistance provided implementation guidance, complete server
code and Python socket test commands. I installed and compiled
the code, restarted the server and ran the tests on my Ubuntu VM.

Tests confirmed burst rejection, recovery after the window,
independent client counters, the shared limit across chat commands,
and continued LIST and QUIT operation. Rejections appeared in the
timestamped server log.

Implementation commit: 1d9a060.

Current limitation: reconnecting resets the counter, and file
transfers and other commands are outside this chat-only limit.
