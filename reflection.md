# NetMessenger Reflection

Registration number: IT23634480

Review note: This is an AI-assisted draft based on the recorded development and tests. I must review the first-person statements and revise them to match my own understanding before submission.

I used ChatGPT/Codex for both explanation and code assistance during NetMessenger development. The assistance included example implementations, debugging guidance, documentation drafts and corrected source files. On October 6, I uploaded my C files after manual tests revealed defects. The assistant produced patches and ran automated regression tests. I installed those files in Ubuntu, compiled them and repeated the relevant tests. This distinction matters: AI-produced code and automated results should be acknowledged separately from tests I performed myself.

Four representative requests shaped the work. First, I asked for a step-by-step explanation of the assignment; this helped organise the mandatory commands and personalised values. Second, I asked how to support several clients; the pthread approach helped me understand the listening socket, accepted sockets and shared state. Third, I asked about partial recv() calls and TCP framing; this clarified why a single receive does not guarantee a complete application message. Fourth, I requested help checking and correcting the application after test failures; that review exposed missing presence notifications, insufficient room permissions and misaligned input following a rejected file transfer. These are summaries of the requests, not verbatim quotations.

The most useful lesson was that successful normal operation does not prove correct error handling. A room message reached members correctly, yet a non-member could also send to that room. Sending a file to an unknown user returned the expected error, but unread payload bytes then produced an extra invalid-command response. The corrected design checks membership and consumes a valid bounded file payload before rejecting its target. I verified both behaviours through the terminal tests.

I also used concrete evidence to check successful transfers. The original, received and server-stored copies of the 23-byte test file had matching sizes and SHA-256 hashes. Five-client tests checked broadcast delivery and private/room isolation. QUIT and Ctrl+C tests checked user removal and leave notifications. Invalid commands were followed by LIST to confirm the connection still worked, and I inspected the listening port and timestamped log.

AI assistance accelerated development, but I remain responsible for understanding and explaining the final code. Before the viva, I need to practise the byte-stream protocol, worker threads, mutexes and cleanup paths. I also recognise the remaining limitations: blocking I/O can delay clients, and the application does not provide TLS, password authentication or persistent history.
