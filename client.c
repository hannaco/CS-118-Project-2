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

// TODO: congestion control, reliable transport
// probably need to add looping to implement reliable delivery -- looping until you get your ACKs

#define MAX_CWND 51200
#define MAX_SEQ 102401

int sockfd;
int fin_flag = 0;

// for the two second timer at the end
void sighandler() {
  close(sockfd);
  if(fin_flag)
  {
    exit(0);
  }
  else
  {
    exit(2);
  }
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
void printRecv(int16_t flag, int32_t serv_seq, int32_t serv_ack, int16_t connection, int cwnd, int ssthresh){
  switch(flag) {
    case 0: printf("RECV %d %d %d %d %d\n", serv_seq, 0, connection, cwnd, ssthresh);
      break;
    case 2: printf("RECV %d %d %d %d %d SYN\n", serv_seq, 0, connection, cwnd, ssthresh);
      break;
    case 4: printf("RECV %d %d %d %d %d ACK\n", serv_seq, serv_ack, connection, cwnd, ssthresh);
      break;
    case 1: printf("RECV %d %d %d %d %d FIN\n", serv_seq, 0, connection, cwnd, ssthresh);
      break;
    case 6: printf("RECV %d %d %d %d %d ACK SYN\n", serv_seq, serv_ack, connection, cwnd, ssthresh);
      break;
    case 3: printf("RECV %d %d %d %d %d SYN FIN\n", serv_seq, 0, connection, cwnd, ssthresh);
      break;
    case 5: printf("RECV %d %d %d %d %d ACK FIN\n", serv_seq, serv_ack, connection, cwnd, ssthresh);
      break;
    default: printf("RECV %d %d %d %d %d\n", serv_seq, 0, connection, cwnd, ssthresh);
  }
}

// printing DROP message based on flags
void printDrop(int16_t flag, int32_t serv_seq, int32_t serv_ack, int16_t connection){
  switch(flag) {
    case 0: printf("DROP %d %d %d\n", serv_seq, 0, connection);
      break;
    case 2: printf("DROP %d %d %d SYN\n", serv_seq, 0, connection);
      break;
    case 4: printf("DROP %d %d %d ACK\n", serv_seq, serv_ack, connection);
      break;
    case 1: printf("DROP %d %d %d FIN\n", serv_seq, 0, connection);
      break;
    case 6: printf("DROP %d %d %d ACK SYN\n", serv_seq, serv_ack, connection);
      break;
    case 3: printf("DROP %d %d %d SYN FIN\n", serv_seq, 0, connection);
      break;
    case 5: printf("DROP %d %d %d ACK FIN\n", serv_seq, serv_ack, connection);
      break;
    default: printf("DROP %d %d %d\n", serv_seq, 0, connection);
  }
}

//adjusting cwnd
int adjustCwnd(int cwnd, int ssthresh)
{
  if(cwnd < ssthresh)
  {
    cwnd += 512;
  }
  else
  {
    cwnd += (512 * 512) / cwnd;
  }

  if(cwnd >  MAX_CWND)
  {
    cwnd = MAX_CWND;
  }

  return cwnd;
}

int main(int argc, char **argv)
{
  int i, portnum, n, fileLength;
  int16_t connection, serv_flag, my_conn;
  int16_t SYN = 0b0000000000000010;
  int16_t ACK = 0b0000000000000100;
  int16_t FIN = 0b0000000000000001;
  int16_t NONE = 0b0000000000000000;
  int32_t ack_num, seq_num, serv_seq, serv_ack;
  FILE* filePointer;
  size_t res, itr, sendSize, cwnd_end;
  unsigned int len;
  char* sendbuf;
  char* header;
  char buffer[525];
  struct hostent *server;
  struct sockaddr_in servaddr;
  int cwnd = 512;
  int ssthresh = 10000;
  int packetTable[201] = {0};

  // checking that we have both port number and file dir as args
  if(argc < 4) {
    fprintf(stderr, "ERROR: Not enough arguments to server.\n");
    exit(1);
  }
  // storing host name
  char* hostname = argv[1];

  // setting up alarm for timeout
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
    fprintf(stderr, "ERROR: Not a valid filename in client. %s\n", argv[3]);
    exit(1);
  }
  // getting file size
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

  // creating socket
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
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
  // ten second alarm
  alarm(10);
  printf("SEND %d %d %d %d %d SYN\n", seq_num, 0, connection, cwnd, ssthresh);
  seq_num++;
  // receive SYN-ACK from server
  n = recvfrom(sockfd, (char *)buffer, 525, 0, (struct sockaddr *) &servaddr, &len);
  // reset ten second alarm
  alarm(10);
  buffer[n] = '\0';
  // decoding the header
  serv_seq = getSeq(buffer);
  serv_ack = getAck(buffer);
  serv_flag = getFlags(buffer);
  connection = getConnection(buffer);
  my_conn = connection;
  printRecv(serv_flag, serv_seq, serv_ack, connection, cwnd, ssthresh);

  // for(int i = 0; i < 12; i++){
  //   printf("%d\n", buffer[i]);
  // }

  // send ACK with payload
  ack_num = (serv_seq+1) % MAX_SEQ;
  header = makeHeader(seq_num, ack_num, connection, ACK);
  memset(buffer, 0, sizeof buffer);
  memcpy(buffer, header, 12);
  // if our file is more than 512 bytes then we only send 512 bytes
  if(res > 512){
    memcpy(buffer+12, sendbuf, 512);
    buffer[524] = '\0';
    sendSize = 524;
  } else {
    // otherwise we send the whole thing
    memcpy(buffer+12, sendbuf, res);
    buffer[res+12] = '\0';
    sendSize = 12+res;
  }
  sendto(sockfd, (const char *)buffer, sendSize, 0, (const struct sockaddr *) &servaddr,  sizeof(servaddr));
  printf("SEND %d %d %d %d %d ACK\n", seq_num, ack_num, connection, cwnd, ssthresh);
  seq_num = (seq_num + sendSize - 12) % MAX_SEQ;
  // receive ACK from server
  n = recvfrom(sockfd, (char *)buffer, 525, 0, (struct sockaddr *) &servaddr, &len);
  // reset ten second alarm
  alarm(10);
  buffer[n] = '\0';
  // decoding the header
  serv_seq = getSeq(buffer);
  serv_ack = getAck(buffer);
  serv_flag = getFlags(buffer);
  connection = getConnection(buffer);
  // if the connection ID doesn't match we drop it
  if(connection != my_conn){
    printDrop(serv_flag, serv_seq, serv_ack, my_conn);
  } else {
    printRecv(serv_flag, serv_seq, serv_ack, connection, cwnd, ssthresh);
    cwnd = adjustCwnd(cwnd, ssthresh);
    ack_num = (serv_seq+1) % MAX_SEQ;
  }
  
  fd_set rfds;

  // if we have more data to send
  if(res > 512) {
    // send file in 512 byte payloads (need to get ACKs)
    itr = 512;
    // this is the last unacked byte
    int sendBase = seq_num;
    // the number of the ack we expect
    int expAck = seq_num;
    int dup = 0;
    // while we have 512 byte chunks to send
    while(itr+512 <= res) {
      printf("%ld\n", itr);
      printf("%ld\n", res);
      if(itr <= res-512) printf("%ld <= %ld\n", itr, res-512);
      // we set sequence number to the last unacked byte because that is the next one we are sending
      seq_num = sendBase;

      int bytesSent = 0;
      int startItr = itr;

      // while(bytesSent <= cwnd - 512 && itr <= res-512)
      while(bytesSent <= 0)
      {
        //store file position of this packet
        packetTable[seq_num/512] = itr;

        //make packet
        header = makeHeader(seq_num, ack_num, connection, NONE);
        memset(buffer, 0, sizeof buffer);
        memcpy(buffer, header, 12);
        memcpy(buffer+12, sendbuf+itr, 512);
        buffer[524] = '\0';
        sendSize = 524;
        sendto(sockfd, (const char *)buffer, sendSize, 0, (const struct sockaddr *) &servaddr,  sizeof(servaddr));

        // if this is a duplicate packet
        if(dup)
        {
          dup = 0;
          printf("SEND %d %d %d %d %d DUP\n", seq_num, 0, connection, cwnd, ssthresh);
          fprintf(stderr, "DUP PACKET: seq_num = %d, sendBase= %d\n", seq_num, sendBase);
        }
        else
        {
          printf("SEND %d %d %d %d %d\n", seq_num, 0, connection, cwnd, ssthresh);
        }

        itr += 512;
        bytesSent += 512;
        seq_num = (seq_num + sendSize - 12) % MAX_SEQ;
      }
      
      // printf("SEND %d %d %d %d %d\n", seq_num, 0, connection, cwnd, ssthresh);

      //check if dup
      

      //update expected ack number
      // expAck = (seq_num + sendSize - 12) % MAX_SEQ;
      expAck = seq_num;

      
      //new additions
      struct timeval tv;

      tv.tv_sec = 0;
      tv.tv_usec = 500000;

      FD_ZERO(&rfds);
      FD_SET(sockfd, &rfds);

      int retval = select(sockfd+1, &rfds, NULL, NULL, &tv);

      while(retval)
      {
        n = recvfrom(sockfd, (char *)buffer, 525, 0, (struct sockaddr *) &servaddr, &len);

          // reset ten second alarm
        alarm(10);
        buffer[n] = '\0';
        // decoding the header
        serv_seq = getSeq(buffer);
        serv_ack = getAck(buffer);
        serv_flag = getFlags(buffer);
        connection = getConnection(buffer);
        // if the connection ID doesn't match we drop it
        if(connection != my_conn){
          printDrop(serv_flag, serv_seq, serv_ack, my_conn);
        } else {
          // seq_num = (seq_num + sendSize - 12) % MAX_SEQ;
          ack_num = (serv_seq+1) % MAX_SEQ;
          printRecv(serv_flag, serv_seq, serv_ack, connection, cwnd, ssthresh);
          cwnd = adjustCwnd(cwnd, ssthresh);

          int lastSuccess;
          
          if(serv_ack  < 512)
          {
            lastSuccess = serv_ack - 512 + MAX_SEQ;
          }
          else
          {
            lastSuccess = serv_ack - 512;
          }

          if(serv_ack <= sendBase)
          {
            fprintf(stderr, "GREATER OR EQUAL ACK serv_ack = %d, sendBase= %d, lastSuccess= %d\n", serv_ack, sendBase, lastSuccess);
          }

          // fprintf(stderr, "INDICES serv_ack ind = %d, sendBase ind = %d\n", serv_ack/512, sendBase/512);

          if(packetTable[lastSuccess/512] >= packetTable[sendBase/512])
          {
            sendBase = serv_ack;
          }
          else
          {
            fprintf(stderr, "OUT OF ORDER ACK serv_ack = %d, sendBase= %d, server itr = %d, sendBase itr = %d\n", serv_ack, sendBase, packetTable[lastSuccess/512],packetTable[sendBase/512] );
          }
        }

        tv.tv_sec = 0;
        tv.tv_usec = 500000;

        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);

        if(sendBase == expAck)
        {
          // itr+=512;
          break;
        }

        retval = select(sockfd+1, &rfds, NULL, NULL, &tv);

      }

      if(sendBase != expAck)
      {
        fprintf(stderr, "MISSING ACK for seq num = %d, sendBase= %d, and expAck = %d, last serv ack = %d\n", seq_num, sendBase, expAck, serv_ack);
        ssthresh = cwnd/2;
        cwnd = 512;
        dup = 1;
        itr = packetTable[sendBase/512];
      }
    }
    // itr -= 512;
    header = makeHeader(seq_num, ack_num, connection, NONE);
    memset(buffer, 0, sizeof buffer);
    memcpy(buffer, header, 12);
    memcpy(buffer+12, sendbuf+itr, res-itr);
    buffer[res-itr+12] = '\0';
    sendSize = res-itr+12;
    sendto(sockfd, (const char *)buffer, sendSize, 0, (const struct sockaddr *) &servaddr,  sizeof(servaddr));
    printf("SEND %d %d %d %d %d\n", seq_num, 0, connection, cwnd, ssthresh);
    // receive ACK
    n = recvfrom(sockfd, (char *)buffer, 525, 0, (struct sockaddr *) &servaddr, &len);
    // reset ten second alarm
    alarm(10);
    buffer[n] = '\0';
    // decoding the header
    serv_seq = getSeq(buffer);
    serv_ack = getAck(buffer);
    serv_flag = getFlags(buffer);
    connection = getConnection(buffer);
    // if the connection ID doesn't match we drop it
    if(connection != my_conn){
      printDrop(serv_flag, serv_seq, serv_ack, my_conn);
    } else {
      ack_num = (serv_seq+1) % MAX_SEQ;
      seq_num = (seq_num + sendSize - 12) % MAX_SEQ;
      printRecv(serv_flag, serv_seq, serv_ack, connection, cwnd, ssthresh);
      cwnd = adjustCwnd(cwnd, ssthresh);
    }
  }

  // send FIN
  header = makeHeader(seq_num, ack_num, connection, FIN);
  sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &servaddr,  sizeof(servaddr));
  printf("SEND %d %d %d %d %d FIN \n", seq_num, 0, connection, cwnd, ssthresh);
  seq_num++;

  // receive ACK or FIN_ACK
  n = recvfrom(sockfd, (char *)buffer, 525, 0, (struct sockaddr *) &servaddr, &len);
  // two second timer
  fin_flag = 1;
  alarm(2);
  buffer[n] = '\0';
  // decoding the header
  serv_seq = getSeq(buffer);
  serv_ack = getAck(buffer);
  serv_flag = getFlags(buffer);
  connection = getConnection(buffer);
  // if the connection ID doesn't match we drop it
  if(connection != my_conn){
    printDrop(serv_flag, serv_seq, serv_ack, my_conn);
  } else {
    printRecv(serv_flag, serv_seq, serv_ack, connection, cwnd, ssthresh);
    cwnd = adjustCwnd(cwnd, ssthresh);
    ack_num = (serv_seq+1) % MAX_SEQ;
    // if it was a FIN_ACK, we need to ACK
    if(serv_flag == FIN+ACK) {
      header = makeHeader(seq_num, ack_num, connection, ACK);
      sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &servaddr,  sizeof(servaddr));
      printf("SEND %d %d %d %d %d ACK \n", seq_num, ack_num, connection, cwnd, ssthresh);
      seq_num++;
    }
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
    ack_num = (serv_seq+1) % MAX_SEQ;
    if(serv_flag == FIN) {
      printRecv(serv_flag, serv_seq, serv_ack, connection, cwnd, ssthresh);
      cwnd = adjustCwnd(cwnd, ssthresh);
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