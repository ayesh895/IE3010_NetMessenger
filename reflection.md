# NetMessenger Reflection

## Registration Number
IT23634480

During this assignment, I used ChatGPT as a learning and development
assistant. I did not use it only to generate a final program. Instead,
I followed a step-by-step process where I implemented and tested each
feature before moving to the next one. AI was mainly used to help me
understand the assignment specification, design the client-server
architecture, explain socket programming concepts, suggest code
structures, debug compiler errors, and organise the final
documentation.

The most useful AI support was the explanation of TCP socket
communication and concurrency. It helped me understand the purpose of
socket(), bind(), listen(), accept(), connect(), pthreads and mutexes.
It was also useful when implementing TCP framing. I learned that TCP is
a byte stream and one recv() call does not necessarily correspond to
one complete application message. This led to the implementation of
recv_line(), recv_exact() and send_all().

AI output was not always directly usable. One example was when
recv_line() was referenced before the compiler had seen its
declaration. This produced an implicit declaration and conflicting type
compiler error. I had to identify the cause, move or declare the
function correctly, and then recompile. This showed me that generated
code still has to be understood, compiled and verified rather than
copied without checking.

I also modified and tested the file-transfer design carefully. The
SENDFILE command had to be followed by an exact number of raw bytes.
After implementing the transfer, I created a 32-byte test file and
compared the SHA-256 hash of the original file, the server-stored copy,
and the receiver copy. All three hashes matched. This gave me
confidence that the transfer was complete and unmodified.

The assignment improved my understanding of multi-client network
programming. I learned how a server can use one listening socket while
accept() returns a different connected socket for each client. I also
learned why shared user and room data need mutex protection in a
threaded server. Testing unexpected client termination helped me
understand why connection cleanup is important.

Overall, AI was most useful as a guide for design, explanation and
debugging, but the important learning came from implementing,
compiling, testing, fixing errors and verifying the behaviour myself.
