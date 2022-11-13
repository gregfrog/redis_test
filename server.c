#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <hiredis.h>

char *hostname = "127.0.0.1";
int port = 6379;
struct timeval timeout = {1, 500000};

void *socketThread(void *arg)
{
  char client_message[2000];
  char timestampBuffer[50];
  char redisCommandText[100];

  int newSocket = *((int *)arg);
  free(arg);
  int bytesReceived = recv(newSocket, client_message, sizeof(client_message) - 1, 0);
  if (bytesReceived == 0)
  {
    printf("zero bytes read\n");
  }
  if (bytesReceived == -1)
  {
    printf("read error %d\n", errno);
  }
  client_message[bytesReceived] = 0;

  // get high resolution timestamp
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  unsigned long usNow = (unsigned long)(ts.tv_sec * 1000000L + ts.tv_nsec / 1000L);
  sprintf(timestampBuffer, "%lu", usNow);

  redisContext *c = redisConnectWithTimeout(hostname, port, timeout);
  if (c == NULL || c->err)
  {
    if (c)
    {
      printf("Redis Connection error: %s\n", c->errstr);
      redisFree(c);
    }
    else
    {
      printf("Redis Connection error: can't allocate redis context\n");
    }
    exit(1);
  }

  char *messageBody = strstr(client_message, "HELLO");
  if (messageBody == NULL)
  {
    printf("no HELLO in message %s\n", client_message ? client_message : "");
    exit(1);
  }
  char *messageValue = strstr(client_message, "MSGNO");
  if (messageValue == NULL)
  {
    printf("no MSGNO in message %s\n", client_message ? client_message : "");
    exit(1);
  }
  messageValue[-1] = '\0'; // MSGNO comes after the HELLO in the message text.

  redisReply *reply;

  if (strstr(client_message, "LOGON") != NULL)
  {
    //    check for existing record (message 1 has no predecessor), Save body to redis
    if (strcmp(messageValue, "MSGNO1") != 0)
    {
      sprintf(redisCommandText, "GET %s", messageBody);
      reply = redisCommand(c, "GET %s", messageBody);
      if (reply->type != REDIS_REPLY_NIL)
      {
        printf("LOGON missing logoff %lu %s %s %s\n", usNow, reply->str, redisCommandText, messageBody);
      }
      printf("LOGON GET %s: %s\n", messageBody, reply->str);
      freeReplyObject(reply);
    }
    sprintf(redisCommandText, "SET %s %s", messageBody, messageValue);
    reply = redisCommand(c, redisCommandText);
    printf("LOGON SET %s: %s\n", messageBody, reply->str);
    freeReplyObject(reply);
  }
  if (strstr(client_message, "LOGOFF") != NULL)
  {
    //    delete message body from redis
    sprintf(redisCommandText, "GET %s", messageBody);
    reply = redisCommand(c, redisCommandText);
    if (reply->type == REDIS_REPLY_NIL)
    {
      printf("LOGOFF record not found %lu %s %s\n", usNow, messageBody, redisCommandText);
    }
    printf("LOGOFF GET %s: %s\n", messageBody, reply->str);
    freeReplyObject(reply);
    sprintf(redisCommandText, "DEL %s", messageBody);
    reply = redisCommand(c, redisCommandText);
    printf("LOGOFF DEL %s: %s\n", messageBody, reply->str);
    freeReplyObject(reply);
  }
  else
  {
    sprintf(redisCommandText, "GET %s", messageBody);

    reply = redisCommand(c, redisCommandText);
    if (reply->type == REDIS_REPLY_NIL)
    {
      printf("not logged on %lu %s\n", usNow, messageBody);
    }
    printf("%s: %s\n", redisCommandText, reply->str);
    freeReplyObject(reply);
    sprintf(redisCommandText, "SET %s %s", messageBody, messageValue);
    reply = redisCommand(c, redisCommandText);
    printf("%s: %s\n", redisCommandText, reply->str);
    freeReplyObject(reply);
  }

  // Send message to the client socket
  char *messagePrefix = "Hello Client : ";
  char *message = malloc(sizeof(client_message) + strlen(messagePrefix));
  strcpy(message, messagePrefix);
  strcat(message, client_message);
  printf("%s\n", message);
  int sendrc = send(newSocket, message, strlen(message), 0);
  if (sendrc == -1)
  {
    printf("send error %d\n", errno);
  }
  free(message);

  redisFree(c);

  int closerc = close(newSocket);
  if (closerc == -1)
  {
    printf("read error %d\n", errno);
  }
  pthread_exit(NULL);
}

int main()
{
  int serverSocket;
  struct sockaddr_in serverAddr;
  struct sockaddr_storage serverStorage;
  socklen_t addr_size;

  // Create the socket.
  serverSocket = socket(PF_INET, SOCK_STREAM, 0);

  // Configure settings of the server address struct
  // Address family = Internet
  serverAddr.sin_family = AF_INET;

  // Set port number, using htons function to use proper byte order
  serverAddr.sin_port = htons(7799);

  // Set IP address to localhost
  serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

  // Set all bits of the padding field to 0
  memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

  // Bind the address struct to the socket
  bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

  // Listen on the socket, with 40 max connection requests queued
  if (listen(serverSocket, 400) == 0)
  {
    printf("Listening\n");
  }
  else
  {
    printf("Error %d\n", errno);
  }

  pthread_t tid[410];
  int i = 0;
  while (1)
  {
    // Accept call creates a new socket for the incoming connection
    addr_size = sizeof serverStorage;
    int newSocket = accept(serverSocket, (struct sockaddr *)&serverStorage, &addr_size);
    if (newSocket == -1)
    {
      printf("socket error %d\n", errno);
      abort();
    }

    int *sockPointer = malloc(sizeof(newSocket));
    *sockPointer = newSocket;
    int pthcErrno = pthread_create(&tid[i++], NULL, socketThread, sockPointer);
    if (pthcErrno != 0)
    {
      printf("Failed to create thread %d\n", pthcErrno);
    }

    if (i >= 400)
    {
      i = 0;
      while (i < 400)
      {
        int errorNumber = pthread_join(tid[i++], NULL);
        if (errorNumber != 0)
        {
          printf("pthread_join error %d\n", errorNumber);
        }
      }
      i = 0;
    }
  }
  return 0;
}