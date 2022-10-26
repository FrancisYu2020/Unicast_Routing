#include <vector>
#include <set>
#include <iostream>
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
// #include <jsoncons/json.hpp>

#include "monitor_neighbors.h"

// namespace jc = jsoncons;
using namespace std;

#define neighbor_LEN 256
#define timeout 700 * 1000 //TODO: set a reasonable timeout value, 700ms currently as suggested on campuswire
// #define _BSD_SOURCE

unsigned int **costMatrix;
// int **isAdjacent;
pthread_mutex_t costMatrixLock;

void set_cost(int source, int target, unsigned int newCost) {
  costMatrix[source][target] = newCost;
  costMatrix[target][source] = newCost;
}
unsigned int get_cost(int source, int target) {
  return costMatrix[source][target];
}

void print_costMatrix() {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      printf("%d ", costMatrix[i][j]);
    }
    printf("\n");
  }
}

void init_cost() {
  costMatrix = (unsigned int **)malloc(neighbor_LEN * sizeof(int *));
  for (int i=0; i < neighbor_LEN; i++) {
    costMatrix[i] = (unsigned int *)malloc(neighbor_LEN * sizeof(unsigned int));
    for (int j = 0; j < neighbor_LEN; j++) {
      costMatrix[i][j] = 0;
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

unsigned int time_elapse(struct timeval start, struct timeval end) {
  return (end.tv_sec - start.tv_sec) * 1000 * 1000 + (end.tv_usec - start.tv_usec);
}

void *check_neighbors_alive(void* arg) {
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
      if (i == globalMyID || forwardingTable[i].nexthop != i) continue;
      unsigned int timeDiff = time_elapse(globalLastHeartbeat[i], currTime);
      if (timeDiff > timeout) {
        //TODO: handle lost connection case
        // set_cost(globalMyID, i, 0);
        forwardingTable[i].dist = -1;
        forwardingTable[i].seqNum += 1;
        forwardingTable[i].nexthop = -1;
        forwardingTable[i].isNeighbor = 0;
      }
    }
		nanosleep(&sleepFor, 0);
	}
}

void *listenForNeighbors(void* arg)
{
  printf("In monitor_neighbors.h int globalMyID = %d\n", globalMyID);

	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[8192];

	int bytesRecvd;
	while(1)
	{
		theirAddrLen = sizeof(theirAddr);
    // printf("In while\n" );
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 8192 , 0,
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
    // printf("debug in while in listenForNeighbors\n");

		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);

		short int heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
      // printf("%d\n", heardFrom);

			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.
      forwardingTable[heardFrom].isNeighbor = 1;
			//record that we heard from heardFrom just now.
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
      // cout << globalLastHeartbeat[heardFrom].tv_sec << " " << globalLastHeartbeat[heardFrom].tv_usec << endl;
		}

		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp((const char *)recvBuf, "send", 4))
		{
			//TODO send the requested message to the requested destination node
      uint16_t no_destID;
      char msg[8192];
      unsigned char forwardBuf[8192];
      memcpy(&no_destID, recvBuf + 4, sizeof(short));
      memcpy(msg, recvBuf + 4 + sizeof(short int), 8192);
      memcpy(forwardBuf, "forward", 7);
      memcpy(forwardBuf + 7, recvBuf + 4, 8192);
      short int destID = ntohs(no_destID);
      short int nexthopID = forwardingTable[destID].nexthop;
      char log_msg[8192];
      if (nexthopID == -1) {
        //TODO: log unreachable destination and drop the message
        sprintf(log_msg, "unreachable dest %hd\n", destID);
      }
      else {

        sprintf(log_msg, "sending packet dest %hd nexthop %hd message %s\n", destID, nexthopID, msg);

        //TODO: send out the message
        // sendto(globalSocketUDP, forwardBuf, 8192, 0, \
          (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
      }
      //TODO: finish the log
      write_log(log_msg);
		}
		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp((const char *)recvBuf, "cost", 4))
		{
			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			// ...
      uint16_t no_destID;
      uint32_t no_newCost;
      memcpy(&no_destID, recvBuf + 4, sizeof(uint16_t));
      memcpy(&no_newCost, recvBuf + 4 + sizeof(short), sizeof(uint32_t));
      // printf("debug4\n" );
      short int destID = ntohs(no_destID);
      int newCost = ntohl(no_newCost);
      forwardingTable[destID].cost = newCost;
      forwardingTable[destID].seqNum += 1;
      dijkstra(globalMyID);

      // // TODO: log this event
		}

		//TODO now check for the various types of packets you use in your own protocol
		//else if(!strncmp(recvBuf, "your other message types", ))
		// ...
    else if (!strncmp((const char *)recvBuf, "forward", 7))
    {
      // TODO: update the heartbeat information
      uint16_t no_destID;
      char msg[8192];
      unsigned char forwardBuf[8192];
      memcpy(&no_destID, recvBuf + 4, sizeof(short));
      memcpy(msg, recvBuf + 4 + sizeof(short int), 8192);
      memcpy(forwardBuf, "forward", 7);
      memcpy(forwardBuf + 7, recvBuf + 4, 8192);
      short int destID = ntohs(no_destID);
      short int nexthopID = forwardingTable[destID].nexthop;
      char log_msg[8192];
      if (destID == globalMyID) {
        sprintf(log_msg, "receive packet message %s\n", msg);
      }
      else if (nexthopID == -1) {
        //TODO: log unreachable destination and drop the message
        sprintf(log_msg, "unreachable dest %hd\n", destID);
      }
      else {

        sprintf(log_msg, "forward packet dest %hd nexthop %hd message %s\n", destID, nexthopID, msg);

        //TODO: send out the message
        sendto(globalSocketUDP, forwardBuf, 8192, 0, \
          (struct sockaddr*)&globalNodeAddrs[nexthopID], sizeof(globalNodeAddrs[nexthopID]));
      }
      //TODO: finish the log
      write_log(log_msg);
    }
    else if (!strncmp((const char *)recvBuf, "LSA", 3)) {
      int neighborID;
      // struct tableItem *neighborFT = decode_structure(recvBuf, &neighborID);
      int update;
      decode_structure(recvBuf, &update);
      if (update) {
        hackyBroadcast1(recvBuf, 8192);
      }
    }
    else if (!strncmp((const char *)recvBuf, "print", 5)) {
      cout << "fuck" << endl;
      print_costMatrix();
    }
	}
	//(should never reach here)
	close(globalSocketUDP);
}

void dijkstra(int root) {
  unsigned int dist[neighbor_LEN];
  int parents[neighbor_LEN];
  set<int> explored;
  unsigned int minCost = -1;
  int v = -1;
  explored.insert(root);

  for (int i = 0; i < neighbor_LEN; i ++) {
    if (i == root) continue;
    // if (forwardingTable[i].connected) {
    //   dist[i] = get_cost(root, i);
    //   if (dist[i] < minCost) {
    //     //TODO: tie breaker: the smaller index will be selected if there is a tie in cost)
    //     minCost = dist[i];
    //     v = i;
    //   }
    //   parents[i] = root;
    // }
    else dist[i] = -1;
  }
  //loop
  while(explored.size() < neighbor_LEN) {
    //TODO
    explored.insert(v);

  }
}

unsigned char *encode_structure(short int *sendIdx, short int counter) {
  // send message: LSA<3 bytes> + sourceID<4 bytes> + counter<4 bytes> + counter * (id + blocks)
  unsigned char *msg = (unsigned char*)malloc(3 + 4 + 2 + counter * (2 + sizeof(struct tableItem)));
  memcpy(msg, "LSA", 3);
  memcpy(msg + 3, &globalMyID, 4);
  memcpy(msg + 7, &counter, 2);
  unsigned char *tmp = msg + 9;
  for (int i = 0;i < counter; i ++) {
    unsigned char block[2 + sizeof(struct tableItem)];
    memcpy(block, &sendIdx[i], 2);
    memcpy(block + 2, &forwardingTable[sendIdx[i]], sizeof(struct tableItem));
    memcpy(tmp + i * (2 + sizeof(struct tableItem)), block, 2 + sizeof(struct tableItem));
  }
  return msg;
}

void decode_structure(unsigned char *msg, int *update) {
  //decode the message from neighbors and update global information accordingly
  int sourceID;
  int changed = 0;
  short int counter;
  unsigned char *tmp = (unsigned char *)(msg + 3);
  memcpy(&sourceID, tmp, 4);
  if (sourceID == globalMyID) {
    *update = 0;
    return;
  }
  memcpy(&counter, tmp + 4, 2);
  tmp += 6;

  for (int i = 0; i < counter; i ++) {
    short int entryIdx;
    struct tableItem entry;
    memcpy(&entryIdx, tmp, 2);
    tmp += 2;
    memcpy(&entry, tmp, 2 + sizeof(struct tableItem));
    tmp += 2 + sizeof(struct tableItem);
    unsigned int oldCost = get_cost(sourceID, entryIdx);
    if (oldCost != entry.cost) {
      changed += 1;
      set_cost(sourceID, entryIdx, entry.cost);
    }
    // if (oldCost != entry.cost || (oldCost == 0 && entry.isNeighbor)) {
    //   changed += 1;
    //   set_cost(sourceID, entryIdx, entry.cost);
    // }
    // else if (oldCost && (entry.isNeighbor == 0)) {
    //   //find the source is disconnected to the modified entry
    //   changed += 1;
    //   set_cost(sourceID, entryIdx, 0);
    // }
  }
  memcpy(update, &changed, 4);
}

// void *send_LSA(void *unusedParam){
//   while(1) {
//
//   }
// }

//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<neighbor_LEN;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0, (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void hackyBroadcast1(const unsigned char* buf, int length)
{
	int i;
	for(i=0;i<neighbor_LEN;i++)
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
    short int sendIdx[neighbor_LEN], counter;
    for (int i = 0; i < neighbor_LEN; i ++) {
      if (forwardingTable[i].seqNum > oldSeq[i]) {
        sendIdx[counter] = i;
        counter ++;
        oldSeq[i] = forwardingTable[i].seqNum;
      }
    }
    if (counter) {
      unsigned char *msg = encode_structure(sendIdx, counter);
      //TODO: send the LSA to other nodes
      hackyBroadcast1((const unsigned char *)msg, 11 + sizeof(struct tableItem) * counter);
      //TODO: find out if we need to free the message
      free(msg);
    }
    else {
      hackyBroadcast("HEREIAM", 7);
    }
		nanosleep(&sleepFor, 0);
	}
}
