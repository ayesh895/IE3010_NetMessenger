# NetMessenger Design Diary

## Student
Registration Number: IT23634480

## Initial Planning
I reviewed the assignment specification and identified the required
protocol commands and personalisation rules. From my registration
number, I calculated the TCP port as 10480 and the Node ID as NID:6344.

I selected a thread-per-client server architecture using POSIX pthreads.
This approach allowed the server to continue accepting new connections
while each connected client was handled independently.

## Basic TCP Communication
I first created a basic TCP server using socket(), bind(), listen() and
accept(). Then I created the TCP client using socket() and connect().
The connection was tested locally using 127.0.0.1 and port 10480.

## Registration and Client Management
I implemented REGISTER first and then added a shared connected-client
table. A pthread mutex was used to protect the table from concurrent
access. Duplicate usernames were detected and rejected with
ERR 001 USERNAME_TAKEN.

## Messaging
LIST was implemented to return all currently connected users. I then
implemented BCAST and PMSG. The client was updated with a separate
receiver thread so that it could receive messages while the main thread
continued accepting user commands.

## Room Management
I created a room structure containing the room name and member socket
list. JOIN creates a room when necessary, LEAVE removes a client from a
room, ROOMS lists available rooms, and RMSG forwards messages only to
room members.

## TCP Framing
An important issue was that TCP does not preserve application message
boundaries. I implemented recv_line() for newline-terminated protocol
messages and send_all() to make sure an entire output buffer is sent.

## File Transfer
SENDFILE was one of the most challenging parts. I implemented
recv_exact() so that the exact declared number of raw file bytes is
received even when several recv() calls are required.

The server stores a copy using the personalised path:

./storage/IT23634480/<sender_username>/<filename>

Files can be sent to an individual user or to a room.

I verified file integrity using SHA-256. The original file, the server
copy and the receiver copy produced the same hash.

## Error and Disconnect Handling
I tested duplicate usernames, unknown users, unknown rooms, invalid
commands, graceful QUIT and unexpected client termination. When a
client disconnects unexpectedly, the server removes the user from the
active client list and room memberships.

## Logging and Final Testing
I added timestamped logging to netmsg_IT23634480.log. The log records
connections, registrations, messages, file transfers and disconnect
events.

Finally, I tested the server with five simultaneous clients and
confirmed all five connected users with LIST.
