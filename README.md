CS118 Project 2

Isha Gonugunta (405337405)
    - Congestion control
    - Connection timeout
    - Retransmission
    - Receive window
    - Server writing to saved file
    - Conversion from C to C++
Hanna Co (205303784)
    - Set up client and server
    - Connection creation
    - Creating packets
    - Packet transmission
    - Timeout
Helen Wang (405320396)
    - Packet transmission
    - Retransmission timeout
    - README

Design

The server is started with command line command ./server [portnum] ./save; it checks for valid portnum and creates a UDP socket on the port if it's valid. We have a while loop that continuously loops to serve requests from the client and use select to monitor the socket file descriptor for input from the client without blocking. Upon receiving packet input, the server decodes the packet received to get the sequence number, ack number, and packet type. It looks up the connection by connectionId, creates a file to save if it's the start of the 3 way handshake with the SYN packet, and starts with initial sequence number 4321;  we have an array and class to store information about each connection created. For each packet response, we call a helper makeHeader function to create the packet to send as response to the client and write the received data to the created file in chunks. Upon receiving the FIN packet to end from the client, the server sends back the response packets to acknowledge and closes the file being written to, it gracefully exits when the user terminates the server run on the command line.

client:
The client checks for valid hostname, port number, and file, then opens the client socket, opens the requested file, and initiates the 3 way handshake to open the connection with the socket. We use alarm to check if the client recieves a packet back from the socket witin 10 seconds and close the connection if not and wait for 2 seconds at the end of the transfer to close the connection. We have helper functions that create a packet header and decode the sequence num, ack num, packet type, and connection id with bit masking and call during packet creation in the loop to send packets to the socket. The client has a packet table indexed by the sequence number to keep track of the file position for the data chunk that each packet should have. For cwnd, we keep track of the cwnd and ssthresh and adjust according to slow start and congestion avoidance guidelines. For retransmission for duplicate packets, we keep track of the earliest successful packet that has yet to be acked, the most recent successful send, and the expected ack during each iteration and check to see if a duplicate packet has arrived to indicate retransmission or if a greater than expected ack has arrived to indicate a lost packet/gap was filled. 

Problems
One problem we had was that we didn't know the test files included binary and thought the transmitted files would just be regular text, causing some issues since we used strcmp, strlength, and other functions from the string library that doesn't work with binary input and making us fail cases we expected to pass. After realising that the test files had binary, we replaced all usages of string functions and fixed the issue.

We also had issues with the send and receive output not matching the output of the autograder, which we figured out by running the referential implementation client and server and our client and server and using diff to compare the outputs. That was helpful for debugging and we found issues with formatting, adjusting the congestion window, the output not being printed, and different send/receive output order which we were then able to fix.

We had issues with timeout and retransmission. To make sure we timed out after 10 seconds for the server aborting the connection and for when there are multiple connections, we added a class to store connection information and keep track of aborting connections after timeout. For retransmission we were incorrectly transmitting some of the duplicate lost packets and debugged to find that the file position iterator for some of the packets wasn't adjusted correctly. 

We had some problems with the environment and autograder. One of our group member's had a laptop with an M1 chip so they weren't able to run the autograder and another group member had a Windows environment and wasn't able to get Virtual Box and loss working and had timing issues with the autograder.


Additional Libraries/Resources
https://www.geeksforgeeks.org/udp-server-client-implementation-c/
https://man7.org/linux/man-pages/man3/getaddrinfo.3.html
https://www.geeksforgeeks.org/time-delay-c/
https://stackoverflow.com/questions/7226603/timeout-function
https://stackoverflow.com/questions/2808398/easily-measure-elapsed-time
