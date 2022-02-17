#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>

int sockfd;

void sighandler() {
  close(sockfd);
  exit(0);
}

char* makeHeader(int32_t seq, int32_t ack, int16_t conn, int16_t flag) {
  int32_t mask_32 = 0b00000000000000000000000011111111;
  int16_t mask_16 = 0b0000000011111111;
  int16_t temp;
  char* header;
  int cwnd = 512;
  int ssthresh = 10000;
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

int32_t getSeq(char* header) {
  int32_t serv_seq = ((header[0]<<24)&0b11111111000000000000000000000000)
                    +((header[1]<<16)&0b00000000111111110000000000000000)
                    +((header[2]<<8)&0b00000000000000001111111100000000)
                    +(header[3]&0b00000000000000000000000011111111);
  return serv_seq;
}

int32_t getAck(char* header) {
  int32_t serv_ack = ((header[4]<<24)&0b11111111000000000000000000000000)
                    +((header[5]<<16)&0b00000000111111110000000000000000)
                    +((header[6]<<8)&0b00000000000000001111111100000000)
                    +(header[7]&0b00000000000000000000000011111111);
  return serv_ack;
}

int16_t getFlags(char* header) {
  int16_t serv_flag = ((header[10]<<8)&0b1111111100000000)+(header[11]&0b0000000011111111);
  return serv_flag;
}

int16_t getConnection(char* header) {
  int16_t connection = ((header[8]<<8)&0b1111111100000000)+(header[9]&0b0000000011111111);
  return connection;
}

void printRecv(int16_t flag, int32_t serv_seq, int32_t serv_ack, int16_t connection, int cwnd, int ssthresh){
  switch(flag) {
    case 0: printf("RECV %d %d %d %d %d\n", serv_seq, serv_ack, connection, cwnd, ssthresh);
      break;
    case 2: printf("RECV %d %d %d %d %d SYN\n", serv_seq, serv_ack, connection, cwnd, ssthresh);
      break;
    case 4: printf("RECV %d %d %d %d %d ACK\n", serv_seq, serv_ack, connection, cwnd, ssthresh);
      break;
    case 1: printf("RECV %d %d %d %d %d FIN\n", serv_seq, serv_ack, connection, cwnd, ssthresh);
      break;
    case 6: printf("RECV %d %d %d %d %d ACK SYN\n", serv_seq, serv_ack, connection, cwnd, ssthresh);
      break;
    case 3: printf("RECV %d %d %d %d %d SYN FIN\n", serv_seq, serv_ack, connection, cwnd, ssthresh);
      break;
    case 5: printf("RECV %d %d %d %d %d ACK FIN\n", serv_seq, serv_ack, connection, cwnd, ssthresh);
      break;
    default: printf("RECV %d %d %d %d %d\n", serv_seq, serv_ack, connection, cwnd, ssthresh);
  }
}

void printDrop(int16_t flag, int32_t serv_seq, int32_t serv_ack, int16_t connection){
  switch(flag) {
    case 0: printf("DROP %d %d %d\n", serv_seq, serv_ack, connection);
      break;
    case 2: printf("DROP %d %d %d SYN\n", serv_seq, serv_ack, connection);
      break;
    case 4: printf("DROP %d %d %d ACK\n", serv_seq, serv_ack, connection);
      break;
    case 1: printf("DROP %d %d %d FIN\n", serv_seq, serv_ack, connection);
      break;
    case 6: printf("DROP %d %d %d ACK SYN\n", serv_seq, serv_ack, connection);
      break;
    case 3: printf("DROP %d %d %d SYN FIN\n", serv_seq, serv_ack, connection);
      break;
    case 5: printf("DROP %d %d %d ACK FIN\n", serv_seq, serv_ack, connection);
      break;
    default: printf("DROP %d %d %d\n", serv_seq, serv_ack, connection);
  }
}

int main(int argc, char **argv)
{
  int i, portnum, n, fileLength;
  int16_t connection, serv_flag;
  int16_t SYN = 0b0000000000000010;
  int16_t ACK = 0b0000000000000100;
  int16_t FIN = 0b0000000000000001;
  int16_t NONE = 0b0000000000000000;
  int32_t MAX_SEQ = 0b00000000000000011001000000000000;
  int32_t ack_num, seq_num, serv_seq, serv_ack;
  FILE* filePointer;
  size_t res, itr, sendSize;
  unsigned int len;
  char* sendbuf;
  char* header;
  char buffer[525];
  struct hostent *server;
  struct sockaddr_in servaddr;
  int cwnd = 512;
  int ssthresh = 10000;
  int close_timer = 1000 * 2;
  int timeout_timer = 1000 * 10;
  clock_t start_time;
  // checking that we have both port number and file dir as args
  if(argc < 4) {
    fprintf(stderr, "ERROR: Not enough arguments to server.\n");
    exit(1);
  }

  char* hostname = argv[1];

  signal(SIGALRM, sighandler);

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
  header = makeHeader(seq_num, ack_num, connection, SYN);

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

  // send SYN to server
  sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &servaddr,  sizeof(servaddr));
  printf("SEND %d %d %d %d %d SYN\n", seq_num, ack_num, connection, cwnd, ssthresh);
  seq_num++;
  // receive SYN-ACK from server
  n = recvfrom(sockfd, (char *)buffer, 525, 0, (struct sockaddr *) &servaddr, &len);
  buffer[n] = '\0';
  // decoding the header
  serv_seq = getSeq(buffer);
  serv_ack = getAck(buffer);
  serv_flag = getFlags(buffer);
  connection = getConnection(buffer);
  printRecv(serv_flag, serv_seq, serv_ack, connection, cwnd, ssthresh);
    

  // for(int i = 0; i < 12; i++){
  //   printf("%d\n", buffer[i]);
  // }

  // send ACK with payload
  ack_num = serv_seq+1;
  header = makeHeader(seq_num, ack_num, connection, ACK);
  memset(buffer, 0, sizeof buffer);
  memcpy(buffer, header, 12);
  if(res > 512){
    memcpy(buffer+12, sendbuf, 512);
    buffer[524] = '\0';
    sendSize = 524;
  } else {
    memcpy(buffer+12, sendbuf, res);
    buffer[res+12] = '\0';
    sendSize = 12+res;
  }
  sendto(sockfd, (const char *)buffer, sendSize, 0, (const struct sockaddr *) &servaddr,  sizeof(servaddr));
  printf("SEND %d %d %d %d %d ACK\n", seq_num, ack_num, connection, cwnd, ssthresh);
  seq_num += sendSize - 12;
  // receive ACK from server
  n = recvfrom(sockfd, (char *)buffer, 525, 0, (struct sockaddr *) &servaddr, &len);
  buffer[n] = '\0';
  // decoding the header
  serv_seq = getSeq(buffer);
  serv_ack = getAck(buffer);
  serv_flag = getFlags(buffer);
  connection = getConnection(buffer);
  printRecv(serv_flag, serv_seq, serv_ack, connection, cwnd, ssthresh);
  ack_num = serv_seq+1;
  

  // send file in 512 byte payloads (need to get ACKs)
  itr = 512;
  while(itr <= res){
    header = makeHeader(seq_num, ack_num, connection, NONE);
    memset(buffer, 0, sizeof buffer);
    memcpy(buffer, header, 12);
    memcpy(buffer+12, sendbuf+itr-1, 512);
    buffer[524] = '\0';
    sendSize = 524;
    sendto(sockfd, (const char *)buffer, sendSize, 0, (const struct sockaddr *) &servaddr,  sizeof(servaddr));
    printf("SEND %d %d %d %d %d\n", seq_num, ack_num, connection, cwnd, ssthresh);
    n = recvfrom(sockfd, (char *)buffer, 525, 0, (struct sockaddr *) &servaddr, &len);
    buffer[n] = '\0';
    // decoding the header
    serv_seq = getSeq(buffer);
    serv_ack = getAck(buffer);
    serv_flag = getFlags(buffer);
    connection = getConnection(buffer);
    seq_num += sendSize - 12;
    ack_num = serv_seq+1;
    printRecv(serv_flag, serv_seq, serv_ack, connection, cwnd, ssthresh);
    itr+=512;
  }
  itr -= 512;
  header = makeHeader(seq_num, ack_num, connection, NONE);
  memset(buffer, 0, sizeof buffer);
  memcpy(buffer, header, 12);
  memcpy(buffer+12, sendbuf+itr-1, res-itr);
  buffer[res-itr+12] = '\0';
  sendSize = res-itr+12;
  sendto(sockfd, (const char *)buffer, sendSize, 0, (const struct sockaddr *) &servaddr,  sizeof(servaddr));
  printf("SEND %d %d %d %d %d\n", seq_num, ack_num, connection, cwnd, ssthresh);
  n = recvfrom(sockfd, (char *)buffer, 525, 0, (struct sockaddr *) &servaddr, &len);
  buffer[n] = '\0';
  // decoding the header
  serv_seq = getSeq(buffer);
  serv_ack = getAck(buffer);
  serv_flag = getFlags(buffer);
  connection = getConnection(buffer);
  ack_num = serv_seq+1;
  seq_num += sendSize - 12;
  printRecv(serv_flag, serv_seq, serv_ack, connection, cwnd, ssthresh);

  // send FIN
  header = makeHeader(seq_num, ack_num, connection, FIN);
  sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &servaddr,  sizeof(servaddr));
  printf("SEND %d %d %d %d %d FIN \n", seq_num, ack_num, connection, cwnd, ssthresh);
  seq_num++;

  // receive ACK or FIN_ACK
  n = recvfrom(sockfd, (char *)buffer, 525, 0, (struct sockaddr *) &servaddr, &len);
  alarm(2);
  buffer[n] = '\0';
  // decoding the header
  serv_seq = getSeq(buffer);
  serv_ack = getAck(buffer);
  serv_flag = getFlags(buffer);
  connection = getConnection(buffer);
  printRecv(serv_flag, serv_seq, serv_ack, connection, cwnd, ssthresh);
  ack_num = serv_seq+1;
  // if it was a FIN_ACK, we need to ACK
  if(serv_flag == FIN+ACK) {
    header = makeHeader(seq_num, ack_num, connection, ACK);
    sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &servaddr,  sizeof(servaddr));
    printf("SEND %d %d %d %d %d ACK \n", seq_num, ack_num, connection, cwnd, ssthresh);
    seq_num++;
  }

  // wait 2 seconds (receiving and ACKing FIN packets, drop others)
  while(1) {
    n = recvfrom(sockfd, (char *)buffer, 525, 0, (struct sockaddr *) &servaddr, &len);
    buffer[n] = '\0';
    // decoding the header
    serv_seq = getSeq(buffer);
    serv_ack = getAck(buffer);
    serv_flag = getFlags(buffer);
    connection = getConnection(buffer);
    ack_num = serv_seq+1;
    if(serv_flag == FIN) {
      printRecv(serv_flag, serv_seq, serv_ack, connection, cwnd, ssthresh);
      header = makeHeader(seq_num, ack_num, connection, ACK);
      sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &servaddr,  sizeof(servaddr));
      printf("SEND %d %d %d %d %d ACK \n", seq_num, ack_num, connection, cwnd, ssthresh);
      seq_num++;
    } else {
      printDrop(serv_flag, serv_seq, serv_ack, connection);
    }
  }
  // // close connection
  // close(sockfd);
  // exit(0);
}