#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  //Header file for sleep(). man 3 sleep for details.
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_SIZE 524

void sighandler() {
  printf("Signal caught.\n");
  exit(0);
}

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
  int i, portnum, sockfd, length;
  int16_t connection;
  int16_t SYN = 0b0000000000000010;
  int16_t ACK = 0b0000000000000100;
  int16_t FIN = 0b0000000000000001;
  int16_t NONE = 0b0000000000000000;
  int32_t ack_num, seq_num;
  unsigned int sz;
  int connection_count = 1;
  struct sockaddr_in my_addr, cli_addr;
  char buffer[MAX_SIZE + 1];
  // checking that we have both port number and file dir as args
  if(argc < 3) {
    fprintf(stderr, "ERROR: Not enough arguments to server.\n");
    exit(1);
  }

  // checking that port number is all digits
  for(i = 0; i < strlen(argv[1]); i++){
    if(!(isdigit(argv[1][i]))) {
      fprintf(stderr, "ERROR: Invalid port number.\n");
      exit(1);
    }
  }
  // converting the port number to an int
  portnum = atoi(argv[1]);

  // listening for signals
  signal(SIGQUIT, sighandler);
  signal(SIGTERM, sighandler);

  // creating socket
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  if (sockfd  == -1)
  {
      fprintf(stderr, "ERROR: error creating socket - %s\r\n", strerror(errno));
      exit(1);
  }

  memset(&my_addr, 0, sizeof(my_addr));
	memset(&cli_addr, 0, sizeof(cli_addr));

  // setting addr info
  my_addr.sin_family = AF_INET; //  IPv4
  my_addr.sin_port = htons(portnum);   // port number
  my_addr.sin_addr.s_addr = INADDR_ANY; //internet addr

  // bind socket to IP addr and port number
  if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
  {
      fprintf(stderr, "ERROR: error binding socket - %s\r\n", strerror(errno));
      exit(1);
  }

  // getting the size of cli_addr
  sz = sizeof(cli_addr);

    // receiving packet from a client
  length = recvfrom(sockfd, (char *) buffer, MAX_SIZE+1, 0, (struct sockaddr *) &cli_addr, &sz);
  if (length < 0) {
    fprintf(stderr, "ERROR: error receiving packet - %s\r\n", strerror(errno));
    exit(1); 
  }

  // setting the last byte to zero so we can write to a file
  buffer[length] = '\0';

  printf("Client : %s\n", buffer);
}
