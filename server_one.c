// #include <string.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>  //Header file for sleep(). man 3 sleep for details.
// #include <pthread.h>
// #include <errno.h>
// #include <ctype.h>
// #include <signal.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <time.h>

// #define MAX_SIZE 524

// typedef struct Request{
// 	int length;
//   unsigned int sz;
// 	struct sockaddr_in cli_addr;
// 	char buffer[MAX_SIZE+1];
//   int connection;
// }Request;

// int sockfd;
// char* folder;

// void sighandler() {
//   exit(0);
// }

// char* makeHeader(int32_t seq, int32_t ack, int16_t conn, int16_t flag) {
//   int32_t mask_32 = 0b00000000000000000000000011111111;
//   int16_t mask_16 = 0b0000000011111111;
//   int16_t temp;
//   char* header;
//   header = malloc(13);
//   header[3] = seq&mask_32;
//   header[2] = (seq>>8)&mask_32;
//   header[1] = (seq>>16)&mask_32;
//   header[0] = (seq>>24)&mask_32;
//   header[7] = ack&mask_32;
//   header[6] = (ack>>8)&mask_32;
//   header[5] = (ack>>16)&mask_32;
//   header[4] = (ack>>24)&mask_32;
//   temp = conn&mask_16;
//   header[9] = temp;
//   temp = (conn>>8)&mask_16;
//   header[8] = temp;
//   temp = flag&mask_16;
//   header[11] = temp;
//   temp = (flag>>8)&mask_16;
//   header[10] = temp;
//   header[12] = '\0';
//   return header;
// }

// int32_t getSeq(char* header) {
//   int32_t serv_seq = ((header[0]<<24)&0b11111111000000000000000000000000)
//                     +((header[1]<<16)&0b00000000111111110000000000000000)
//                     +((header[2]<<8)&0b00000000000000001111111100000000)
//                     +(header[3]&0b00000000000000000000000011111111);
//   return serv_seq;
// }

// int32_t getAck(char* header) {
//   int32_t serv_ack = ((header[4]<<24)&0b11111111000000000000000000000000)
//                     +((header[5]<<16)&0b00000000111111110000000000000000)
//                     +((header[6]<<8)&0b00000000000000001111111100000000)
//                     +(header[7]&0b00000000000000000000000011111111);
//   return serv_ack;
// }

// int16_t getFlags(char* header) {
//   int16_t serv_flag = ((header[10]<<8)&0b1111111100000000)+(header[11]&0b0000000011111111);
//   return serv_flag;
// }

// int16_t getConnection(char* header) {
//   int16_t connection = ((header[8]<<8)&0b1111111100000000)+(header[9]&0b0000000011111111);
//   return connection;
// }

// void printRecv(int16_t flag, int32_t serv_seq, int32_t serv_ack, int16_t connection){
//   switch(flag) {
//     case 0: printf("RECV %d %d %d\n", serv_seq, serv_ack, connection);
//       break;
//     case 2: printf("RECV %d %d %d SYN\n", serv_seq, serv_ack, connection);
//       break;
//     case 4: printf("RECV %d %d %d ACK\n", serv_seq, serv_ack, connection);
//       break;
//     case 1: printf("RECV %d %d %d FIN\n", serv_seq, serv_ack, connection);
//       break;
//     case 6: printf("RECV %d %d %d ACK SYN\n", serv_seq, serv_ack, connection);
//       break;
//     case 3: printf("RECV %d %d %d SYN FIN\n", serv_seq, serv_ack, connection);
//       break;
//     case 5: printf("RECV %d %d %d ACK FIN\n", serv_seq, serv_ack, connection);
//       break;
//     default: printf("RECV %d %d %d\n", serv_seq, serv_ack, connection);
//   }
// }

// void* handle_request(void* req) {
//   FILE* filePointer;
//   int16_t SYN = 0b0000000000000010;
//   int16_t ACK = 0b0000000000000100;
//   int16_t FIN = 0b0000000000000001;
//   int16_t NONE = 0b0000000000000000;
//   int16_t cli_flag, cli_connection;
//   char filename[4096];
//   int32_t ack_num, seq_num, cli_seq, cli_ack;
//   Request *request = (Request *)req;
// 	int length = request->length;
//   unsigned int sz = request->sz;
// 	struct sockaddr_in cli_addr = request->cli_addr;
// 	char buffer[MAX_SIZE+1];
//   char* header;
//   memcpy(buffer, request->buffer, 12);
//   int connection = request->connection;
//   snprintf(filename, sizeof(filename), ".%s/%d.file", folder, connection);
//   filePointer = fopen(filename, "w+");
//   if(filePointer == NULL){
//     fprintf(stderr, "ERROR: Unable to open file.\n");
//     exit(1);
//   }
//   // setting the last byte to zero so we can write to a file
//   buffer[length] = '\0';

//   // getting seq and ack number from clients packet and making a response header
//   cli_seq = getSeq(buffer);
//   cli_ack = getAck(buffer);
//   cli_flag = getFlags(buffer);
//   cli_connection = getConnection(buffer);
//   seq_num = 4321;
//   ack_num = cli_seq+1;
//   header = makeHeader(seq_num, ack_num, connection, SYN+ACK);
//   printRecv(cli_flag, cli_seq, cli_ack, connection);

//   // sends SYN ACK, no payload
//   sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
//   printf("SEND %d %d %d ACK SYN\n", seq_num, ack_num, connection);
//   seq_num++;

//   // receives ACK with some payload
//   length = recvfrom(sockfd, (char *)buffer, MAX_SIZE, 0, (struct sockaddr *) &cli_addr, &sz);
//   buffer[length] = '\0';
//   // decoding the header
//   cli_seq = getSeq(buffer);
//   cli_ack = getAck(buffer);
//   cli_flag = getFlags(buffer);
//   cli_connection = getConnection(buffer);

//   printRecv(cli_flag, cli_seq, cli_ack, cli_connection);
//   if(cli_seq+length > 102400)
//     ack_num = cli_seq+length-12-102400-1;
//   else
//     ack_num = cli_seq+length-12;

//   fputs(buffer+12, filePointer);

//   // sends ACK, no payload
//   header = makeHeader(seq_num, ack_num, cli_connection, ACK);
//   sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
//   printf("SEND %d %d %d ACK\n", seq_num, ack_num, cli_connection);
//   seq_num++;
//   if(cli_seq+length > 102400)
//     ack_num = cli_seq+length-12-102400-1;
//   else
//     ack_num = cli_seq+length-12;

//   // ACK received packets
//   do {
//     length = recvfrom(sockfd, (char *)buffer, MAX_SIZE, 0, (struct sockaddr *) &cli_addr, &sz);
//     buffer[length] = '\0';
//     fputs(buffer+12, filePointer);
//     // decoding the header
//     cli_seq = getSeq(buffer);
//     cli_ack = getAck(buffer);
//     cli_flag = getFlags(buffer);
//     if(cli_flag == FIN)
//       break;
//     printRecv(cli_flag, cli_seq, cli_ack, cli_connection);
//       if(cli_seq+length > 102400)
//         ack_num = cli_seq+length-12-102400-1;
//       else
//         ack_num = cli_seq+length-12;
//     header = makeHeader(seq_num, ack_num, cli_connection, ACK);
//     sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
//     printf("SEND %d %d %d ACK\n", seq_num, ack_num, cli_connection);
//     seq_num++;
//   } while(length != -1);

//   // receive FIN packet
//   printRecv(cli_flag, cli_seq, cli_ack, cli_connection);
//   ack_num++;

//   // send ACK
//   header = makeHeader(seq_num, ack_num, cli_connection, ACK);
//   sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
//   printf("SEND %d %d %d ACK\n", seq_num, ack_num, cli_connection);
//   seq_num++;

//   // send FIN until we receive ACK
//   do {
//     header = makeHeader(seq_num, ack_num, cli_connection, FIN);
//     sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
//     printf("SEND %d %d %d FIN\n", seq_num, ack_num, cli_connection);

//     length = recvfrom(sockfd, (char *)buffer, MAX_SIZE, 0, (struct sockaddr *) &cli_addr, &sz);
//     buffer[length] = '\0';
//     // decoding the header
//     cli_seq = getSeq(buffer);
//     cli_ack = getAck(buffer);
//     cli_flag = getFlags(buffer);
//     cli_connection = getConnection(buffer);

//     printRecv(cli_flag, cli_seq, cli_ack, cli_connection);
//     // if its an ACK
//     if(cli_flag == ACK)
//       break;
//   } while(length != -1);

//   free(request);
// 	pthread_exit(NULL);
// }

// int main(int argc, char **argv)
// {
//   int i, portnum, length, milli_seconds;
//   clock_t start_time;
//   int16_t cli_flag;
//   int16_t SYN = 0b0000000000000010;
//   int16_t ACK = 0b0000000000000100;
//   int16_t FIN = 0b0000000000000001;
//   int16_t NONE = 0b0000000000000000;
//   int32_t ack_num, seq_num, cli_seq, cli_ack;
//   unsigned int sz;
//   int connection_count = 1;
//   struct sockaddr_in my_addr;
//   char buffer[MAX_SIZE + 1];
//   char* header;
//   milli_seconds = 1000 * 10;
//   pthread_t threads[1000];
//   // checking that we have both port number and file dir as args
//   if(argc < 3) {
//     fprintf(stderr, "ERROR: Not enough arguments to server.\n");
//     exit(1);
//   }

//   // checking that port number is all digits
//   for(i = 0; i < strlen(argv[1]); i++){
//     if(!(isdigit(argv[1][i]))) {
//       fprintf(stderr, "ERROR: Invalid port number.\n");
//       exit(1);
//     }
//   }
//   // converting the port number to an int
//   portnum = atoi(argv[1]);
//   folder = argv[2];

//   // listening for signals
//   signal(SIGQUIT, sighandler);
//   signal(SIGTERM, sighandler);

//   // creating socket
//   sockfd = socket(AF_INET, SOCK_DGRAM, 0);

//   if (sockfd  == -1)
//   {
//       fprintf(stderr, "ERROR: error creating socket - %s\r\n", strerror(errno));
//       exit(1);
//   }

//   memset(&my_addr, 0, sizeof(my_addr));
//   // setting addr info
//   my_addr.sin_family = AF_INET; //  IPv4
//   my_addr.sin_port = htons(portnum);   // port number
//   my_addr.sin_addr.s_addr = INADDR_ANY; //internet addr

//   // bind socket to IP addr and port number
//   if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
//   {
//       fprintf(stderr, "ERROR: error binding socket - %s\r\n", strerror(errno));
//       exit(1);
//   }

//   while(1) {
//     Request *request;
//     request = (Request *)malloc(sizeof(Request));

//     // getting the size of cli_addr
//     request->sz = sizeof(struct sockaddr_in);
//     // receiving packet from a client
//     request->length = recvfrom(sockfd, (char *) request->buffer, MAX_SIZE, 0, (struct sockaddr *) &request->cli_addr, &request->sz);
    
//     if (request->length < 0) {
//       fprintf(stderr, "ERROR: error receiving packet - %s\r\n", strerror(errno));
//       exit(1); 
//     }

//     request->connection = connection_count;
//     if(pthread_create(&threads[connection_count-1], NULL, handle_request, (void*)request) != 0){
//       fprintf(stderr, "ERROR: Error serving request.\n");
//       exit(1);
//     } else {
//       connection_count++;
//     }
//   }

//   close(sockfd);
//   exit(0);
// }

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
#include <time.h>

#define MAX_SIZE 524

void sighandler() {
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

void printRecv(int16_t flag, int32_t serv_seq, int32_t serv_ack, int16_t connection){
  switch(flag) {
    case 0: printf("RECV %d %d %d\n", serv_seq, serv_ack, connection);
      break;
    case 2: printf("RECV %d %d %d SYN\n", serv_seq, serv_ack, connection);
      break;
    case 4: printf("RECV %d %d %d ACK\n", serv_seq, serv_ack, connection);
      break;
    case 1: printf("RECV %d %d %d FIN\n", serv_seq, serv_ack, connection);
      break;
    case 6: printf("RECV %d %d %d ACK SYN\n", serv_seq, serv_ack, connection);
      break;
    case 3: printf("RECV %d %d %d SYN FIN\n", serv_seq, serv_ack, connection);
      break;
    case 5: printf("RECV %d %d %d ACK FIN\n", serv_seq, serv_ack, connection);
      break;
    default: printf("RECV %d %d %d\n", serv_seq, serv_ack, connection);
  }
}

int main(int argc, char **argv)
{
  int i, portnum, sockfd, length, milli_seconds;
  clock_t start_time;
  int16_t cli_connection, cli_flag;
  int16_t SYN = 0b0000000000000010;
  int16_t ACK = 0b0000000000000100;
  int16_t FIN = 0b0000000000000001;
  int16_t NONE = 0b0000000000000000;
  int32_t ack_num, seq_num, cli_seq, cli_ack;
  unsigned int sz;
  int connection_count = 1;
  struct sockaddr_in my_addr, cli_addr;
  char buffer[MAX_SIZE + 1];
  char* header;
  FILE* filePointer;
  char filename[4096];
  milli_seconds = 1000 * 10;
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

  while(1) {    
    // getting the size of cli_addr
    sz = sizeof(cli_addr);

    // receiving packet from a client
    length = recvfrom(sockfd, (char *) buffer, MAX_SIZE, 0, (struct sockaddr *) &cli_addr, &sz);

    if (length < 0) {
      fprintf(stderr, "ERROR: error receiving packet - %s\r\n", strerror(errno));
      exit(1); 
    }
    snprintf(filename, sizeof(filename), ".%s/%d.file", argv[2], (connection_count));
    filePointer = fopen(filename, "w+");
    if(filePointer == NULL){
      fprintf(stderr, "ERROR: Unable to open file.\n");
      exit(1);
    }
    // setting the last byte to zero so we can write to a file
    buffer[length] = '\0';

    // getting seq and ack number from clients packet and making a response header
    cli_seq = getSeq(buffer);
    cli_ack = getAck(buffer);
    cli_flag = getFlags(buffer);
    cli_connection = getConnection(buffer);
    seq_num = 4321;
    ack_num = cli_seq+1;
    header = makeHeader(seq_num, ack_num, connection_count, SYN+ACK);
    printRecv(cli_flag, cli_seq, cli_ack, 0);

    // sends SYN ACK, no payload
    sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
    printf("SEND %d %d %d ACK SYN\n", seq_num, ack_num, connection_count);
    seq_num++;
    connection_count++;

    // receives ACK with some payload
    length = recvfrom(sockfd, (char *)buffer, MAX_SIZE, 0, (struct sockaddr *) &cli_addr, &sz);
    buffer[length] = '\0';
    // decoding the header
    cli_seq = getSeq(buffer);
    cli_ack = getAck(buffer);
    cli_flag = getFlags(buffer);
    cli_connection = getConnection(buffer);

    printRecv(cli_flag, cli_seq, cli_ack, cli_connection);
    if(cli_seq+length > 102400)
      ack_num = cli_seq+length-12-102400-1;
    else
      ack_num = cli_seq+length-12;

    fputs(buffer+12, filePointer);

    // sends ACK, no payload
    header = makeHeader(seq_num, ack_num, cli_connection, ACK);
    sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
    printf("SEND %d %d %d ACK\n", seq_num, ack_num, cli_connection);
    seq_num++;
    if(cli_seq+length > 102400)
      ack_num = cli_seq+length-12-102400-1;
    else
      ack_num = cli_seq+length-12;

    // ACK received packets
    while(1) {
      length = recvfrom(sockfd, (char *)buffer, MAX_SIZE, 0, (struct sockaddr *) &cli_addr, &sz);
      buffer[length] = '\0';
      fputs(buffer+12, filePointer);
      // decoding the header
      cli_seq = getSeq(buffer);
      cli_ack = getAck(buffer);
      cli_flag = getFlags(buffer);
      if(cli_flag == FIN)
        break;
      printRecv(cli_flag, cli_seq, cli_ack, cli_connection);
        if(cli_seq+length > 102400)
          ack_num = cli_seq+length-12-102400-1;
        else
          ack_num = cli_seq+length-12;
      header = makeHeader(seq_num, ack_num, cli_connection, ACK);
      sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
      printf("SEND %d %d %d ACK\n", seq_num, ack_num, cli_connection);
      seq_num++;
    }

    // receive FIN packet
    printRecv(cli_flag, cli_seq, cli_ack, cli_connection);
    ack_num++;

    // send ACK
    header = makeHeader(seq_num, ack_num, cli_connection, ACK);
    sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
    printf("SEND %d %d %d ACK\n", seq_num, ack_num, cli_connection);
    seq_num++;

    // send FIN until we receive ACK
    do {
      header = makeHeader(seq_num, ack_num, cli_connection, FIN);
      sendto(sockfd, (const char *)header, 12, 0, (const struct sockaddr *) &cli_addr, sz);
      printf("SEND %d %d %d FIN\n", seq_num, ack_num, cli_connection);

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
    } while(length != -1);

    fclose(filePointer);
  }

  close(sockfd);
  exit(0);
}