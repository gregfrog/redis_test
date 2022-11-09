#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>  // for open
#include <unistd.h> // for close
#include <pthread.h>
#include <signal.h>
#include <errno.h>

void *socketThread(void *arg)
{
  char client_message[2000];
  int newSocket = *((int *)arg);
  free(arg);
  int bytesReceived = recv(newSocket, client_message, sizeof(client_message)-1, 0);
  if(bytesReceived == 0)
  {
    printf("zero bytes read\n");
  }
  if(bytesReceived == -1)
  {
    printf("read error %d\n", errno);
  }
  client_message[bytesReceived] = 0;
  // Send message to the client socket
  char *messagePrefix = "Hello Client : ";
  char *message = malloc(sizeof(client_message) + strlen(messagePrefix));
  strcpy(message, messagePrefix);
  strcat(message, client_message);
  printf("%s\n", message);
  int sendrc = send(newSocket, message, strlen(message), 0);
  if(sendrc == -1)
  {
    printf("send error %d\n", errno);
  }
  free(message);
  int closerc = close(newSocket);
  if(closerc == -1)
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
  if (listen(serverSocket, 50) == 0)
  {
    printf("Listening\n");
  }
  else
  {
    printf("Error %d\n", errno);
  }

  pthread_t tid[60];
  int i = 0;
  while (1)
  {
    // Accept call creates a new socket for the incoming connection
    addr_size = sizeof serverStorage;
    int newSocket = accept(serverSocket, (struct sockaddr *)&serverStorage, &addr_size);

  if(newSocket == -1)
  {
    printf("socket error %d\n", errno);
    abort();
  }
    // for each client request creates a thread and assign the client request to it to process
    // so the main thread can entertain next request
    int *sockPointer = malloc(sizeof(newSocket));
    *sockPointer = newSocket;
    if (pthread_create(&tid[i++], NULL, socketThread, sockPointer) != 0)
      printf("Failed to create thread\n");

    if (i >= 50)
    {
      i = 0;
      while (i < 50)
      {
        int errorNumber = pthread_join(tid[i++], NULL);
        if(errorNumber != 0)
        {
          printf("pthread_join error %d\n",errorNumber);
        }
      }
      i = 0;
    }
  }
  return 0;
}