#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/stat.h>

// TODO: congestion control, reliable transport, multiple clients?, ten second timeout, drop

#define MAX_SIZE 524
#define MAX_SEQ 102401
FILE* filePointer;

// exits when signal is receiv
void sighandler() {
  exit(0);
  fclose(filePointer);
}

// function that bit shifts seq, ack, connection number, and flags to create header
char* makeHeader(int32_t seq, int32_t ack, int16_t conn, int16_t flag) {
  int32_t mask_32 = 0xff;
  int16_t mask_16 = 0xff;
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

// decodes header to get seq
int32_t getSeq(char* header) {
  int32_t serv_seq = ((header[0]<<24)&0xff000000)
                    +((header[1]<<16)&0x00ff0000)
                    +((header[2]<<8)&0x0000ff00)
                    +(header[3]&0x000000ff);
  return serv_seq;
}

// decodes header to get ack
int32_t getAck(char* header) {
  int32_t serv_ack = ((header[4]<<24)&0xff000000)
                    +((header[5]<<16)&0x00ff0000)
                    +((header[6]<<8)&0x0000ff00)
                    +(header[7]&0x000000ff);
  return serv_ack;
}

// decodes header to get flags
int16_t getFlags(char* header) {
  int16_t serv_flag = ((header[10]<<8)&0xff00)+(header[11]&0x00ff);
  return serv_flag;
}

// decodes header to get connection number
int16_t getConnection(char* header) {
  int16_t connection = ((header[8]<<8)&0xff00)+(header[9]&0x00ff);
  return connection;
}

// printing RECV message based on flags
void printRecv(int16_t flag, int32_t cli_seq, int32_t cli_ack, int16_t connection){
  switch(flag) {
    case 0: printf("RECV %d %d %d\n", cli_seq, 0, connection);
      break;
    case 2: printf("RECV %d %d %d SYN\n", cli_seq, 0, connection);
      break;
    case 4: printf("RECV %d %d %d ACK\n", cli_seq, cli_ack, connection);
      break;
    case 1: printf("RECV %d %d %d FIN\n", cli_seq, 0, connection);
      break;
    case 6: printf("RECV %d %d %d ACK SYN\n", cli_seq, cli_ack, connection);
      break;
    case 3: printf("RECV %d %d %d SYN FIN\n", cli_seq, 0, connection);
      break;
    case 5: printf("RECV %d %d %d ACK FIN\n", cli_seq, cli_ack, connection);
      break;
    default: printf("RECV %d %d %d\n", cli_seq, 0, connection);
  }
}

// printing DROP message based on flags
void printDrop(int16_t flag, int32_t cli_seq, int32_t cli_ack, int16_t connection){
  switch(flag) {
    case 0: printf("DROP %d %d %d\n", cli_seq, 0, connection);
      break;
    case 2: printf("DROP %d %d %d SYN\n", cli_seq, 0, connection);
      break;
    case 4: printf("DROP %d %d %d ACK\n", cli_seq, cli_ack, connection);
      break;
    case 1: printf("DROP %d %d %d FIN\n", cli_seq, 0, connection);
      break;
    case 6: printf("DROP %d %d %d ACK SYN\n", cli_seq, cli_ack, connection);
      break;
    case 3: printf("DROP %d %d %d SYN FIN\n", cli_seq, 0, connection);
      break;
    case 5: printf("DROP %d %d %d ACK FIN\n", cli_seq, cli_ack, connection);
      break;
    default: printf("DROP %d %d %d\n", cli_seq, 0, connection);
  }
}

int main(int argc, char **argv)
{
  int i, portnum, sockfd, length;
  clock_t start_time;
  int16_t cli_connection, cli_flag;
  int16_t SYN = 0b0000000000000010;
  int16_t ACK = 0b0000000000000100;
  int16_t FIN = 0b0000000000000001;
  int16_t NONE = 0b0000000000000000;
  int32_t ack_num, seq_num, cli_seq, cli_ack;
  unsigned int sz;
  int connection_count = 0;
  struct sockaddr_in my_addr, cli_addr;
  char buffer[MAX_SIZE + 1];
  char* header;
  // FILE* filePointer;
  char filename[4096];
  char pathname[4096];
  char receiveWindow[201][525] = {0};
  int finack = 1;
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

  // continuously serve requests
  while(1) {    
    // getting the size of cli_addr
    sz = sizeof(cli_addr);

    // receiving packet from a client
    length = recvfrom(sockfd, (char *) buffer, MAX_SIZE, 0, (struct sockaddr *) &cli_addr, &sz);
    if (length < 0) {
      fprintf(stderr, "ERROR: error receiving packet - %s\r\n", strerror(errno));
      exit(1); 
    }
    connection_count++;
    // take folder path and connection number and open a file
    struct stat stats;

    sprintf(pathname, ".%s", argv[2]);

    stat(pathname, &stats);

    if(!S_ISDIR(stats.st_mode))
    {
      mkdir(pathname, 0777);
    }
    

    int dir_size = strlen(pathname);

    // if(pathname[dir_size-1] == '/')
    // {
    //   snprintf(filename, sizeof(filename), ".%s%d.file", argv[2], (connection_count));
    // }
    // else
    // {
    //   snprintf(filename, sizeof(filename), ".%s/%d.file", argv[2], (connection_count));
    // }

    snprintf(filename, sizeof(filename), "%s/%d.file", argv[2], (connection_count));

    filePointer = fopen(filename, "w+");
    if(filePointer == NULL){
      fprintf(stderr, "ERROR: Unable to open file in server: %s. -- %s\n", filename, strerror(errno));
      exit(1);
    }
    // setting the last byte to zero so we can write to a file
    buffer[length] = '\0';

    // decoding header from clients packet and making a response header
    cli_seq = getSeq(buffer);
    cli_ack = getAck(buffer);
    cli_flag = getFlags(buffer);
    cli_connection = getConnection(buffer);
    // always start with sequence number of 4321
    seq_num = 4321;
    // ack is next sequence number we expect
    ack_num = (cli_seq+1) % MAX_SEQ;
    header = makeHeader(seq_num, ack_num, connection_count, SYN+ACK);
    printRecv(cli_flag, cli_seq, cli_ack, 0);

    // sends SYN ACK, no payload
    sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
    printf("SEND %d %d %d ACK SYN\n", seq_num, ack_num, connection_count);
    // increment sequence number and connection
    seq_num = (seq_num +1) % MAX_SEQ;

    int nextToWrite = ack_num;
    int writeIndex = ack_num/512;
    // printf("%d\n", writeIndex);
    // printf("%s\n", receiveWindow[writeIndex]);

    int extra = 0;

    // ACK received packets
    while(1) {
      length = recvfrom(sockfd, (char *)buffer, MAX_SIZE, 0, (struct sockaddr *) &cli_addr, &sz);
      buffer[length] = '\0';
      // fwrite(buffer+12, 1, length-12, filePointer);
      // decoding the header
      cli_seq = getSeq(buffer);
      cli_ack = getAck(buffer);
      cli_flag = getFlags(buffer);
      cli_connection = getConnection(buffer);

      // if we got a FIN, break
      if(cli_flag == FIN)
        break;
      // fprintf(stderr, "RECEIVED PACKET WITH cli_seq = %d, nextToWrite = %d, writeIndex = %d, ack_num = %d\n", cli_seq, nextToWrite, writeIndex, ack_num);

      //check if we should write this to file or not
      if(cli_seq == nextToWrite)
      {
        fwrite(buffer+12, 1, length-12, filePointer);
        // fprintf(stderr, "WRITING PACKET WITH nextToWrite = %d, writeIndex = %d\n", nextToWrite, writeIndex);
        // the next sequence number we should be writing
        nextToWrite = (cli_seq+length-12) % MAX_SEQ;
        // index of the array that we should be writing frmo
        writeIndex = nextToWrite/512;
        while(extra != 0) {
          fprintf(stderr, "WRITING PACKET WITH seq_num = %d, writeIndex = %d\n", nextToWrite, writeIndex);
          fwrite(receiveWindow[writeIndex], 1, length-12, filePointer);
          // fputs(receiveWindow[writeIndex], filePointer);
          memset(receiveWindow[writeIndex],0,sizeof(receiveWindow[writeIndex]));
          // update the indices
          nextToWrite = (nextToWrite+512) % MAX_SEQ;
          writeIndex = nextToWrite/512;
          // nextToWrite = ((writeIndex*512)+length-12) % MAX_SEQ;
          extra--;
        }
        // printf("%d\n", writeIndex);
        // printf("%d\n", nextToWrite);
      } 
      else {
        int endRWNDInd = (writeIndex+100)%201;
        int receivedInd = cli_seq/512;
        if(endRWNDInd < writeIndex){
          if(receivedInd >= writeIndex || receivedInd < endRWNDInd) {
            memcpy(receiveWindow[cli_seq/512], buffer+12, length-12);
            // strcpy(buffer+12, receiveWindow[cli_seq/512]);
          }
        }
        else {
          if(receivedInd >= writeIndex && receivedInd < endRWNDInd){
            memcpy(receiveWindow[cli_seq/512], buffer+12, length-12);
            // strcpy(buffer+12, receiveWindow[cli_seq/512]);
          }
        }
      }

      printRecv(cli_flag, cli_seq, cli_ack, cli_connection);
      // ack_num = (cli_seq+length-12) % MAX_SEQ;
      ack_num = nextToWrite;
      header = makeHeader(seq_num, ack_num, cli_connection, ACK);
      sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
      printf("SEND %d %d %d ACK\n", seq_num, ack_num, cli_connection);
    //   seq_num = (seq_num +1) % MAX_SEQ;
    }

    // receive FIN packet
    printRecv(cli_flag, cli_seq, cli_ack, cli_connection);
    ack_num = (ack_num + 1) % MAX_SEQ;

    // send FIN until we receive ACK
    do {
      // if we should send a finack
      if(finack) {
        header = makeHeader(seq_num, ack_num, cli_connection, FIN+ACK);
        finack = 0;
        sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
        printf("SEND %d %d %d ACK FIN\n", seq_num, ack_num, cli_connection);
      }
      // otherwise we just send a fin
      else {
        header = makeHeader(seq_num, ack_num, cli_connection, FIN);
        sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
        printf("SEND %d %d %d FIN\n", seq_num, ack_num, cli_connection);
      }
      length = recvfrom(sockfd, (char *)buffer, MAX_SIZE, 0, (struct sockaddr *) &cli_addr, &sz);
      buffer[length] = '\0';
      // decoding the header
      cli_seq = getSeq(buffer);
      cli_ack = getAck(buffer);
      cli_flag = getFlags(buffer);
      cli_connection = getConnection(buffer);

      printRecv(cli_flag, cli_seq, cli_ack, cli_connection);
      // if its an ACK
      if(cli_flag == ACK)
        break;
      else if(cli_flag == FIN){
        header = makeHeader(seq_num, ack_num, cli_connection, FIN+ACK);
        finack = 0;
        sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
        printf("SEND %d %d %d ACK FIN\n", seq_num, ack_num, cli_connection);
      }
    } while(length != -1);

    fclose(filePointer);
    finack = 1;
  }

  close(sockfd);
  exit(0);
}
