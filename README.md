CS118 Project 2

Isha Gonugunta (405337405)
Hanna Co (205303784)
Helen Wang (405320396)

Design

server: 
- check for hostname + port num valid, create server socket
- have a while loop to continuously serve requests from client + use select to monitor for input without blocking
- upon input, decode the packet header for the data received from the client to get the seq num, ack, and packet type, create a file if it's a SYN packet starting the 3 way handshake, and otherwise call the makeheader function to create a packet header + send it as response to the client. have a vector to store connectioninfo
- then write packet data to the created file in chunks + send ack fin/fin when client sends fin packet + close, exit when the user terminates the server run

client:
- 



functions to create packet headers

Problems
- running autograder w/ m1 chip + windows
- didn't know the test files included binaries, thought just ascii so weren't passing some cases we expected to
    didn't use string library, used memcpy + memset instead
- send/receive/etc weren't matching even though they were actually working
    compared with git diff w/ ref client + server, found formatting issues to fix
- issues with wrong order of receive + sending packets, issue with one of the variables keeping track of writing
to file
- timer issue for 10 seconds + added connectioninfo class to keep track of closing connections after 10 sec timeout

Additional Libraries/Resources
https://www.geeksforgeeks.org/udp-server-client-implementation-c/
https://man7.org/linux/man-pages/man3/getaddrinfo.3.html
https://www.geeksforgeeks.org/time-delay-c/
https://stackoverflow.com/questions/7226603/timeout-function


