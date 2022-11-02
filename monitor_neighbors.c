#include <vector>
#include <queue>
#include <functional>
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

#define neighborLen 256
#define timeout 100 * 1000 //TODO: set a reasonable timeout value, 700ms currently as suggested on campuswire
#define neighborInfoSize 3 + 2 + 2 + 4 + neighborLen * (2 + 4) // details in encode neighbors

unsigned int **costMatrix;
// int **isAdjacent;
// pthread_mutex_t costMatrixLock;
treeNode spanTree[neighborLen];

//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<neighborLen;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0, (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void set_cost(short source, short target, unsigned int newCost) {
  pthread_mutex_lock(&costMatrixLock);
  costMatrix[source][target] = newCost;
  costMatrix[target][source] = newCost;
  pthread_mutex_unlock(&costMatrixLock);
}
unsigned int get_cost(short source, short target) {
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
  costMatrix = (unsigned int **)malloc(neighborLen * sizeof(int *));
  for (int i=0; i < neighborLen; i++) {
    costMatrix[i] = (unsigned int *)malloc(neighborLen * sizeof(unsigned int));
    for (int j = 0; j < neighborLen; j++) {
      costMatrix[i][j] = 0;
    }
  }
}
int isAdjacent(short src, short dest) {
  if (costMatrix[src][dest]) return 1;
  else return 0;
}

// function for logs
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
    pthread_mutex_lock(&FTlocks[globalMyID]);
    forwardingTable[globalMyID].seqNum++;
    char *msg = (char*)malloc(neighborInfoSize);
    memcpy(msg, "LSA", 3);
    memcpy(msg + 3, &globalMyID, 2);
    memcpy(msg + 7, &forwardingTable[globalMyID].seqNum, 4);
    pthread_mutex_unlock(&FTlocks[globalMyID]);
    char *tmp = msg + 11;
    short counter = 0;
    unsigned int zeroCost = 0;
    for (int i = 0; i < neighborLen; i++) {
      //TODO: exclude those non-neighbors and update the direct link
      if (i == globalMyID || (isAdjacent(globalMyID, i) == 0)) continue;
      unsigned int timeDiff = time_elapse(globalLastHeartbeat[i], currTime);
      // if (globalMyID == 6 && (i == 7)) cout << timeDiff << endl;
      // else if (globalMyID == 7 && (i == 6)) cout << timeDiff << endl;
      if (timeDiff > timeout) {
        //TODO: handle lost connection case
        // cout << "Time out for connection: " << globalMyID << "->" << i << endl;
        set_cost(globalMyID, i, 0);
        // sendLSA();
        memcpy(tmp, &i, 2);
        memcpy(tmp + 2, &zeroCost, 4);
        tmp += 6;
        counter ++;
      }
      // if (globalMyID == 6 && (i == 7)) print_costMatrix();
      // else if (globalMyID == 7 && (i == 6)) print_costMatrix();
    }
    memcpy(msg + 5, &counter, 2);
    broadcast_topology(msg, globalMyID);
    free(msg);
		nanosleep(&sleepFor, 0);
	}
}

void *listenForNeighbors(void* arg)
{
  // printf("In monitor_neighbors.h int globalMyID = %d\n", globalMyID);

	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	char recvBuf[1000];

	int bytesRecvd;
	while(1)
	{
		theirAddrLen = sizeof(theirAddr);
    // printf("In while\n" );
    memset(recvBuf, 0, 1000);
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0,
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
    // printf("debug in while in listenForNeighbors\n");

		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);

		short heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
      // printf("%d\n", heardFrom);

			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.
      if (!isAdjacent(heardFrom, globalMyID)) {
        set_cost(globalMyID, heardFrom, forwardingTable[heardFrom].cost);
        // sendLSA();
      }
			//record that we heard from heardFrom just now.
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
      // cout << globalLastHeartbeat[heardFrom].tv_sec << " " << globalLastHeartbeat[heardFrom].tv_usec << endl;
		}

		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp((const char *)recvBuf, "send", 4))
		{
			//TODO send the requested message to the requested destination node
      // cout << "bytes received = " << bytesRecvd << endl;
      // cout << "size_t size = " << sizeof(size_t) << endl;
      dijkstra();
      uint16_t no_destID;
      memcpy(&no_destID, recvBuf + 4, sizeof(short));
      short destID = ntohs(no_destID);
      char log_msg[256];
      short fromPort = ntohs(theirAddr.sin_port);
      short nextHopID;
      char message[100];
      if (fromPort == 8999) {
        strcpy(message, recvBuf + 6);
        size_t messageLen = strlen(message);
        //TODO: log unreachable destination and drop the message
        if (spanTree[destID].nextHop == -1) {
          sprintf(log_msg, "unreachable dest %hd\n", destID);
          write_log(log_msg);
          continue;
        }
        else {
          nextHopID = spanTree[destID].nextHop;
          sprintf(log_msg, "sending packet dest %hd nexthop %hd message %s\n", destID, spanTree[destID].nextHop, message);
          memcpy(recvBuf + 6, &messageLen, sizeof(size_t));
          memcpy(recvBuf + 6 + sizeof(size_t), message, messageLen);
          char *tmp = recvBuf + 6 + sizeof(size_t) + messageLen;
          // cout << "ground truth: ";
          for (short i = 0; i < neighborLen; i ++) {
            memcpy(tmp, &spanTree[i].parent, 2);
            tmp += 2;
            // cout << spanTree[i].parent << " ";
          }
          // cout << endl;
          // tmp = recvBuf + 6 + sizeof(size_t) + messageLen;
          // cout << "internally decoded: ";
          // for (short i = 0; i < neighborLen; i ++) {
          //   short x;
          //   memcpy(&x, tmp, 2);
          //   tmp += 2;
          //   cout << (x) << " ";
          // }
          // cout << endl;
        }
        // cout << "forward to nextHop: "<<nextHopID << endl;
      }
      else if (destID == globalMyID) {
        // cout << "receive in " << globalMyID << endl;
        size_t messageLen;
        memcpy(&messageLen, recvBuf + 6, sizeof(size_t));
        memcpy(message, recvBuf + 6 + sizeof(size_t), messageLen);
        message[messageLen] = '\0';
        sprintf(log_msg, "receive packet message %s\n", message);
        write_log(log_msg);
        continue;
      }
      else {
        //TODO: forward the message
        cout << "forward in " << globalMyID << endl;
        size_t messageLen;
        memcpy(&messageLen, recvBuf + 6, sizeof(size_t));
        memcpy(message, recvBuf + 6 + sizeof(size_t), messageLen);
        message[messageLen] = '\0';
        char *tmp = recvBuf + 6 + sizeof(size_t) + messageLen;
        short dests[neighborLen], parents[neighborLen];
        cout << "externally decoded: ";
        for (short i = 0; i < neighborLen; i ++) {
          memcpy(&parents[i], tmp, 2);
          tmp += 2;
          cout << parents[i] << " ";
        }
        cout << endl;
        short curr = destID;
        short parent = parents[curr];
        while (parent != globalMyID) {
          curr = parent;
          parent = parents[curr];
        }
        sprintf(log_msg, "forward packet dest %hd nexthop %hd message %s\n", destID, curr, message);
        nextHopID = curr;
        // cout << "forward to nextHop: "<<nextHopID << endl;
      }
      // cout << msg << endl;
      //TODO: send out the message
      sendto(globalSocketUDP, recvBuf, 1000, 0,\
         (struct sockaddr*)&globalNodeAddrs[nextHopID], sizeof(globalNodeAddrs[nextHopID]));
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
      short destID = ntohs(no_destID);
      int newCost = ntohl(no_newCost);
      set_cost(globalMyID, destID, newCost);
      forwardingTable[destID].cost = newCost;
      // forwardingTable[destID].seqNum += 1;
      // sendLSA();
      // dijkstra(globalMyID);

      // // TODO: log this event
		}

		//TODO now check for the various types of packets you use in your own protocol
		//else if(!strncmp(recvBuf, "your other message types", ))
		// ...
    else if (!strncmp((const char *)recvBuf, "LSA", 3)) {
      short sourceID;
      int seqNum;
      decode_topology(recvBuf, &seqNum, &sourceID);
      // cout << "in globalMyID = " << globalMyID << ", sending source = " << sourceID << " with seqNum = " << seqNum<< endl;
      if (seqNum > forwardingTable[sourceID].seqNum) {
        forwardingTable[sourceID].seqNum = seqNum;
        broadcast_topology(recvBuf, heardFrom);
      }
    }
    else if (!strncmp((const char *)recvBuf, "print", 5)) {
      // cout << "in globalMyID = " << globalMyID << endl;
      print_costMatrix();
      cout << endl;
    }
	}
	//(should never reach here)
	close(globalSocketUDP);
}

char *encode_neighbors() {
  // cout << "encoding neighbors in nodeID = " << globalMyID << endl;
  // send message: LSA<3 bytes> + sourceID<2 bytes> + counter<2 bytes> + seqNum<4 bytes> + counter * (id + blocks)
  char *msg = (char*)malloc(neighborInfoSize);
  memcpy(msg, "LSA", 3);
  memcpy(msg + 3, &globalMyID, 2);
  memcpy(msg + 7, &forwardingTable[globalMyID].seqNum, 4);
  char *tmp = msg + 11;
  short counter = 0;
  for (short i = 0;i < neighborLen; i ++) {
    // cout << globalMyID << " and " << i << " isAdjacent = " << isAdjacent(globalMyID, i) << endl;
    if (!isAdjacent(globalMyID, i) || (i == globalMyID)) continue;
    // char block[6];
    unsigned int cost = get_cost(globalMyID, i);

    // memcpy(block, &i, 2);
    // memcpy(block + 2, &cost, 4);
    memcpy(tmp, &i, 2);
    memcpy(tmp + 2, &cost, 4);
    tmp += 6;
    counter ++;
  }
  memcpy(msg + 5, &counter, 2);
  return msg;
}
void decode_topology(char *msg, int *seqNum, short *sourceID) {
  //decode the message from neighbors and update global information accordingly
  short targetID, counter;
  unsigned int cost;
  char *tmp = msg + 3;
  memcpy(seqNum, tmp + 4, 4);
  memcpy(sourceID, tmp, 2);
  if (*sourceID == globalMyID || (*seqNum <= forwardingTable[*sourceID].seqNum)) return;
  memcpy(&counter, tmp + 2, 2);
  tmp += 8;
  // vector<int> costs;
  // vector<int> neighbors;
  for (short i = 0; i < counter; i ++) {
    memcpy(&targetID, tmp, 2);
    memcpy(&cost, tmp + 2, 4);
    set_cost(*sourceID, targetID, cost);
    tmp += 6;
  }
  // forwardingTable[*sourceID].seqNum = *seqNum;
  // if (globalMyID == 1) {
  //   // cout << "received "<< sourceID << "'s' LSA broadcast from " << heardFrom << ", and broad cast to neighbors" << endl;
  //   cout << "sourceID = " << *sourceID << " " << counter << " old seq = " << forwardingTable[*sourceID].seqNum << " new seq = " << *seqNum << endl;
  //   for (int i=0; i < counter; i ++) {
  //     cout << "(" << neighbors[i] << ", " << costs[i] << ")" << " ";
  //   }
  //   cout << endl;
  // }
  // if (globalMyID == 1) {
  //   cout << "In globalMyID = " << globalMyID << endl;
  //   print_costMatrix();
  //   cout << endl;
  //   cout << endl;
  // }
}

void broadcast_topology(const char* neighborInfo, short origin)
{
	int i;
	for(i=0;i<neighborLen;i++)
		if(i != globalMyID && (i != origin) && isAdjacent(i, globalMyID)) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, neighborInfo, neighborInfoSize, 0, (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void* announceToNeighbors(void* unusedParam)
{
  // printf("In monitor_neighbors.h int globalMyID = %d\n", globalMyID);

	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	while(1)
	{
    pthread_mutex_lock(&FTlocks[globalMyID]);
    forwardingTable[globalMyID].seqNum++;
    pthread_mutex_unlock(&FTlocks[globalMyID]);
    hackyBroadcast("HEREIAM", 7);
		nanosleep(&sleepFor, 0);
	}
}

void *send_neighbor_costs(void *unusedParam) {
  struct timeval currTime;
  gettimeofday(&currTime, NULL);
  srand((unsigned int)(currTime.tv_sec * 1000000 + currTime.tv_usec));
  struct timespec randomSleep;
  randomSleep.tv_sec = 0;
  // randomSleep.tv_nsec = 300 * 1000 * 1000;
  randomSleep.tv_nsec = (rand() % 1000) * 1000 * 1000;
  cout << (randomSleep.tv_nsec)<< endl;
  nanosleep(&randomSleep, 0);

  struct timespec sleepFor;
  sleepFor.tv_sec = 1;
  sleepFor.tv_nsec = 0;
  while(1)
  {
      pthread_mutex_lock(&FTlocks[globalMyID]);
      forwardingTable[globalMyID].seqNum++;
      char *neighborInfo = encode_neighbors();
      pthread_mutex_unlock(&FTlocks[globalMyID]);
      broadcast_topology(neighborInfo, globalMyID);
      free(neighborInfo);
      nanosleep(&sleepFor, 0);
  }
  // return NULL;
}

bool Compare(treeNode *a, treeNode *b) {
  if (a->dist > b->dist) return true;
  else if (a->dist < b->dist) return false;
  // dist is the same
  if (a->destID > b->dist) return true;
  return false;
}

void dijkstra() {
  priority_queue<treeNode*, vector<treeNode*>, function<bool(treeNode*, treeNode*)>> pq(Compare);
  // treeNode *spanTree = (treeNode *)malloc(neighborLen * sizeof(treeNode));
  for (short i = 0; i < neighborLen; i ++) {
    spanTree[i].destID = i;
    spanTree[i].dist = -1;
    spanTree[i].parent = -1;
    spanTree[i].nextHop = -1;
  }
  spanTree[globalMyID].dist = 0;
  set<short> currSpan;
  // cout << "debug dijkstra" << endl;
  pq.push(&spanTree[globalMyID]);
  while (!pq.empty()) {
    treeNode *curr = pq.top();
    // cout << "curr id is " << curr->destID << endl;
    pq.pop();
    for (short i = 0; i < neighborLen; i ++) {
      unsigned int linkDist;
      if ((linkDist = get_cost(curr->destID, i)) && (currSpan.find(i) == currSpan.end())) {
        unsigned int newCost = linkDist + curr->dist;
        if (spanTree[i].dist > newCost) {
          spanTree[i].parent = curr->destID;
          spanTree[i].dist = linkDist + curr->dist;
          if (curr->destID == globalMyID) spanTree[i].nextHop = i;
          else spanTree[i].nextHop = curr->nextHop;
        }
        else if (spanTree[i].dist == newCost && (spanTree[i].nextHop > curr->nextHop)) {
          spanTree[i].parent = curr->destID;
          spanTree[i].nextHop = curr->nextHop;
        }
        pq.push(&spanTree[i]);
      }
    }
    currSpan.insert(curr->destID);
  }
  // print spanTree details
  // for (short i = 0; i < neighborLen; i ++){
  //   if ((int)spanTree[i].dist == -1) continue;
  //   else {
  //     cout << "(" << (spanTree[i].destID) << "," << spanTree[i].parent << "," << spanTree[i].nextHop << "," << (spanTree[i].dist) << ")";
  //   }
  // }
  // cout << endl;
}
