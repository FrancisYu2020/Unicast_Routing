#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/time.h>
#include "monitor_neighbors.h"

#define neighbor_LEN 256
#define timeout 700 * 1000 //TODO: set a reasonable timeout value, 700ms currently as suggested on campuswire
// #define _BSD_SOURCE

unsigned int **costMatrix;
int **isAdjacent;

void set_cost(int source, int target, unsigned int newCost) {
  costMatrix[source][target] = newCost;
}
unsigned int get_cost(int source, int target) {
  return costMatrix[source][target];
}

void print_costMatrix() {
  for (int i = 0; i < neighbor_LEN; i++) {
    for (int j = 0; j < neighbor_LEN; j++) {
      printf("%d ", costMatrix[i][j]);
    }
    printf("\n");
  }
  printf("%d\n", costMatrix[5][1]);
  costMatrix[5][1] = 100;
  printf("%d\n", costMatrix[5][1]);
}

void init_cost() {
  costMatrix = (unsigned int *)malloc(neighbor_LEN * sizeof(int *));
  for (int i=0; i < neighbor_LEN; i++) {
    costMatrix[i] = (unsigned int *)malloc(neighbor_LEN * sizeof(unsigned int));
    for (int j = 0; j < neighbor_LEN; j++) {
      costMatrix[i][j] = 0;
    }
  }
  // init adjacency matrix
  isAdjacent = (int *)malloc(neighbor_LEN * sizeof(int *));
  for (int i = 0; i < neighbor_LEN; i++) {
    isAdjacent[i] = (int *)malloc(neighbor_LEN * sizeof(int));
    for (int j = 0; j < neighbor_LEN; j++) {
      if (i == j) isAdjacent[i][j] = 1;
      else isAdjacent[i][j] = 0;
    }
  }
}

void write_log(char* log_msg) {
  FILE *fp = fopen(filename, "a");
  if (fp == NULL) {
    fprintf(stderr, "Error in writing log file: %s\n", filename);
    exit(1);
  }
  fputs(log_msg, fp);
  fclose(fp);
}

//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0, (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void* announceToNeighbors(void* unusedParam)
{
  // printf("In monitor_neighbors.h int globalMyID = %d\n", globalMyID);

	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	while(1)
	{
		hackyBroadcast("HEREIAM", 7);
		nanosleep(&sleepFor, 0);
	}
}

unsigned int time_elapse(struct timeval start, struct timeval end) {
  return (end.tv_sec - start.tv_sec) * 1000 * 1000 + (end.tv_usec - start.tv_usec);
}

void check_neighbors_alive() {
  //TODO: check whether a direct link to a neighbor still exists using TTL
  struct timespec sleepFor;
  struct timeval currTime;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 500 * 1000 * 1000; //500 ms
	while(1)
	{
    gettimeofday(&currTime, 0);
    for (int i = 0; i < neighbor_LEN; i++) {
      //TODO: exclude those non-neighbors and update the direct link
      if (i == globalMyID) continue;
      unsigned int timeDiff = time_elapse(globalLastHeartbeat[i], currTime);
      if (timeDiff > timeout) {
        //TODO: handle lost connection case
        set_cost(globalMyID, i, 0)
      }
    }
		nanosleep(&sleepFor, 0);
	}
}

void listenForNeighbors()
{
  printf("In monitor_neighbors.h int globalMyID = %d\n", globalMyID);

	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[1000];

	int bytesRecvd;
	while(1)
	{
		theirAddrLen = sizeof(theirAddr);
    printf("In while\n" );
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0,
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
    printf("debug in while in listenForNeighbors\n");

		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);

		short int heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
      printf("%d\n", heardFrom);

			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.

			//record that we heard from heardFrom just now.
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
		}

		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp(recvBuf, "send", 4))
		{
			//TODO send the requested message to the requested destination node
      uint16_t no_destID;
      char msg[100];
      memset(&no_destID, recvBuf + 4, sizeof(short));
      strcpy(msg, recvBuf + 4 + sizeof(short int));
      short int destID = ntohs(no_destID);
      short int nexthopID = 0; // TODO: find out the nexthopID

      char log_msg[256];
      sprintf(log_msg, "sending packet dest %hd nexthop %hd message %s\n", destID, nexthopID, msg);
      //TODO: finish the log
      write_log(log_msg);
		}
		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp(recvBuf, "cost", 4))
		{
			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			// ...
      // printf("%s\n", recvBuf);
      // printf("debug1\n" );
      uint16_t no_destID;
      uint32_t no_newCost;
      memcpy(&no_destID, recvBuf + 4, sizeof(uint16_t));
      memcpy(&no_newCost, recvBuf + 4 + sizeof(short), sizeof(uint32_t));
      printf("debug4\n" );
      short int destID = ntohs(no_destID);
      int newCost = ntohl(no_newCost);
      // printf("destID = %hd\n", destID);
      // printf("newCost = %d\n", newCost);
      costMatrix[globalMyID][destID] = newCost;
      // printf("debug5.5 %d\n", globalMyID);
      costMatrix[destID][globalMyID] = newCost;
      // printf("debug6\n" );
      // TODO: log this event
		}

		//TODO now check for the various types of packets you use in your own protocol
		//else if(!strncmp(recvBuf, "your other message types", ))
		// ...
    else if (!strncmp(recvBuf, "LSA", 3))
    {
      // TODO: update the heartbeat information
    }
	}
	//(should never reach here)
	close(globalSocketUDP);
}
