# AI Prompt Log - NetMessenger

## Student
Registration Number: IT23634480

## AI Tool Used
ChatGPT / Codex

Record note: The earlier prompt entries below summarise requests and use; they are not verified verbatim transcripts. October 6 assistance included direct source-code patches and automated tests, as recorded in Interaction 12.

---

## Interaction 1 - Assignment Understanding and Planning

### Prompt
Explain the IE3010 NetMessenger assignment and teach me how to complete
it step by step.

### How the AI Output Was Used
The AI helped me identify the mandatory requirements, personalised
values, development stages and evidence that needed to be collected.

### My Evaluation / Changes
I used the assignment specification to verify the requirements and
followed a staged implementation instead of generating the complete
application at once.

---

## Interaction 2 - Basic TCP Server and Client

### Prompt
Help me create the basic TCP server and client using my personalised
port.

### How the AI Output Was Used
The AI explained socket(), bind(), listen(), accept(), connect(),
htons() and SO_REUSEADDR and provided example code.

### My Evaluation / Changes
I compiled and tested each stage on Linux before continuing and kept
screenshots of the successful connection.

---

## Interaction 3 - Multi-Client Concurrency

### Prompt
Help me modify the server to support multiple simultaneous clients.

### How the AI Output Was Used
The AI suggested a pthread-per-client architecture and explained the
difference between the listening socket and connected client sockets.

### My Evaluation / Changes
I implemented the pthread approach, tested two simultaneous clients,
and later verified five simultaneous clients.

---

## Interaction 4 - Registration and User Management

### Prompt
Help me implement REGISTER, duplicate username checking and the
connected-user list.

### How the AI Output Was Used
The AI suggested a shared client structure and mutex protection.

### My Evaluation / Changes
I tested valid registration and duplicate registration separately and
confirmed ERR 001 USERNAME_TAKEN was returned correctly.

---

## Interaction 5 - Messaging Commands

### Prompt
Help me implement LIST, BCAST and PMSG.

### How the AI Output Was Used
The AI provided guidance for broadcasting to all clients except the
sender and private-message lookup by username.

### My Evaluation / Changes
I added a receiver thread to the client so incoming messages could be
received while commands were still being entered. I tested successful
private messages and USER_NOT_FOUND errors.

---

## Interaction 6 - Chat Rooms

### Prompt
Help me implement JOIN, LEAVE, ROOMS and RMSG.

### How the AI Output Was Used
The AI suggested a room structure containing a room name and member
socket list.

### My Evaluation / Changes
I tested room creation, two-user membership, room listing, room
messaging, leaving a room and ROOM_NOT_FOUND handling.

---

## Interaction 7 - TCP Framing

### Prompt
Explain how to correctly handle partial recv() calls and multiple TCP
messages.

### How the AI Output Was Used
The AI suggested recv_line(), recv_exact() and send_all() helper
functions.

### My Evaluation / Changes
I initially received a compiler error because recv_line() was defined
after it was used. I corrected the function placement/prototype and
recompiled successfully.

---

## Interaction 8 - File Transfer

### Prompt
Help me implement SENDFILE with exact byte counting and personalised
server storage.

### How the AI Output Was Used
The AI helped with reading the file size, sending raw bytes, receiving
exactly the declared byte count, storing the server copy and forwarding
the file to users or room members.

### My Evaluation / Changes
I created a 32-byte test file and verified the original file, server
copy and receiver copy using SHA-256. All hashes matched.

---

## Interaction 9 - QUIT, Disconnects and Error Handling

### Prompt
Help me implement QUIT and test graceful and unexpected disconnects.

### How the AI Output Was Used
The AI provided guidance for returning OK BYE, closing the connection
and cleaning up the connected-user and room state.

### My Evaluation / Changes
I tested QUIT, Ctrl+C unexpected termination, invalid commands, unknown
users and unknown rooms. The server continued running after the tests.

---

## Interaction 10 - Server Logging

### Prompt
Help me add timestamped server-side logging.

### How the AI Output Was Used
The AI suggested a mutex-protected log function using time(),
localtime_r() and strftime().

### My Evaluation / Changes
I generated real events and checked netmsg_IT23634480.log. The log
contained server startup, connections, registrations, messages, file
transfer and quit events with timestamps.

---

## Interaction 11 - Documentation Review

### Prompt
Help me structure the README, design diary, prompt log and final report.

### How the AI Output Was Used
The AI suggested headings and content based on the assignment
requirements and the implementation evidence collected during testing.

### My Evaluation / Changes
I kept the documentation specific to my actual implementation and
screenshots rather than describing features that were not tested.

---

## Interaction 12 — October 6, 2026: Source review and defect correction

### Request summary
I asked the assistant to help test whether my app met the assignment options, then provided source files after failures were identified.

### How the AI output was used
The assistant directly patched server_4480.c and client_4480.c. Changes included join/leave notifications, room sender membership checks, rejected-file payload handling, serialization of file headers/payloads, safer input/path validation and improved disconnect handling. It compiled the sources and ran 13 automated regression test groups in its own environment.

### My evaluation / changes
I installed the corrected files in Ubuntu, compiled them and retested presence notifications, graceful/abrupt cleanup, user and room transfers, rejection paths, hash equality, five clients, broadcast delivery, private/room isolation, LEAVE, malformed commands, the listening port and logs. I pushed the corrections as commit ab309c8. TEST_RESULTS.txt describes the assistant's automated tests; my screenshots show my separate manual tests.

## Interaction 13 — October 6, 2026: Documentation update

### Request summary
I continued the assignment review by uploading the project ZIP so the assistant could check the README, diary, prompt log and reflection.

### How the AI output was used
The assistant found that the README ended inside its compilation code block and that the documents omitted the October 6 fixes. It drafted updated English documentation and an explicit account of AI code assistance.

### My evaluation / changes
The reflection is a draft to review against my own understanding. Documentation does not replace the final report, required screenshots or viva preparation.
