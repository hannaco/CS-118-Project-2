#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  //Header file for sleep(). man 3 sleep for details.
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

char* makeHeader(int32_t seq, int32_t ack, int16_t conn, int16_t flag) {
  int32_t mask_32 = 0b00000000000000000000000011111111;
  int16_t mask_16 = 0b0000000011111111;
  int16_t temp;
  char* header;
  header = malloc(13);
  header[3] = seq&mask_32;
  header[2] = (seq>>8)&mask_32;
  header[1] = (seq>>16)&mask_32;
  header[0] = (seq>>24)&mask_32;
  header[7] = ack&mask_32;
  header[6] = (ack>>8)&mask_32;
  header[5] = (ack>>16)&mask_32;
  header[4] = (ack>>24)&mask_32;
  temp = conn&mask_16;
  header[9] = temp;
  temp = (conn>>8)&mask_16;
  header[8] = temp;
  temp = flag&mask_16;
  header[11] = temp;
  temp = (flag>>8)&mask_16;
  header[10] = temp;
  header[12] = '\0';
  return header;
}

int main(int argc, char **argv)
{
  int i, portnum, s, sockfd, fileLength;
  int16_t connection;
  int16_t SYN = 0b0000000000000010;
  int16_t ACK = 0b0000000000000100;
  int16_t FIN = 0b0000000000000001;
  int16_t NONE = 0b0000000000000000;
  int32_t ack_num, seq_num;
  FILE* filePointer;
  size_t res;
  char* sendbuf;
  char* header;
  struct hostent *server;
  struct sockaddr_in servaddr;
  // checking that we have both port number and file dir as args
  if(argc < 4) {
    fprintf(stderr, "ERROR: Not enough arguments to server.\n");
    exit(1);
  }

  char* hostname = argv[1];

  // checking that port number is all digits
  for(i = 0; i < strlen(argv[2]); i++){
    if(!(isdigit(argv[2][i]))) {
      fprintf(stderr, "ERROR: Invalid port number.\n");
      exit(1);
    }
  }
  // converting the port number to an int
  portnum = atoi(argv[2]);

  // opening file
  filePointer = fopen(argv[3], "rb");
  if(filePointer == NULL){
    fprintf(stderr, "ERROR: Not a valid filename.\n");
    exit(1);
  }
  fseek (filePointer, 0, SEEK_END);
  fileLength = ftell(filePointer);
  rewind(filePointer);
  sendbuf = (char*) malloc (sizeof(char)*fileLength);
  res = fread(sendbuf, 1, fileLength, filePointer);
  if(res != fileLength) {
    fprintf(stderr, "ERROR: Problem reading file.\n");
    exit(1);
  }

  // initial header values
  ack_num = 0;
  connection = 0;
  seq_num = 12345;

  // creating the header
  header = makeHeader(seq_num, ack_num, connection, ACK);

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
      fprintf(stderr, "ERROR: error creating socket - %s\r\n", strerror(errno));
      exit(1);
  }

  memset(&servaddr, 0, sizeof(servaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(portnum);
  // gets ip addr from hostname
  server = gethostbyname(hostname);

  if(server == NULL) {
      fprintf(stderr, "ERROR: Invalid host name.");
      exit(1);
  }

  // copy ip address from server into servaddr
  memcpy(&servaddr.sin_addr.s_addr, server->h_addr, server->h_length);

  char *hello = "Hello from client";

  sendto(sockfd, (const char *)hello, strlen(hello), 0, (const struct sockaddr *) &servaddr,  sizeof(servaddr));

  printf("Hello message sent.\n");
           int n, len;
           char buffer[1024];
    n = recvfrom(sockfd, (char *)buffer, 1024, 
                0, (struct sockaddr *) &servaddr,
                &len);
    buffer[n] = '\0';
    printf("Server : %s\n", buffer);
   
    close(sockfd);
    return 0;
  
  // receive SYN-ACK from server

  // send ACK with payload
  // send file in 512 byte payloads (need to get ACKs)

  // send FIN

  // receive ACK

  // wait 2 seconds (receiving and ACKing FIN packets, drop others)

  // close connection

  exit(0);
}