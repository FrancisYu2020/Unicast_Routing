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

extern short globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];

extern char *filename;
extern struct tableEntry forwardingTable[256];

struct tableEntry {
	unsigned int cost;
	int seqNum;
};

struct treeNode {
	short destID;
	unsigned int dist;
	short parent;
	short nextHop;
};

void hackyBroadcast(const char* buf, int length);
void broadcast_topology(const char* neighborInfo, short origin);

void* announceToNeighbors(void* unusedParam);

void *listenForNeighbors(void* arg);

void print_costMatrix();
void init_cost();
unsigned int get_cost(short source, short target);
void set_cost(short source, short target, unsigned int newCost);
int isAdjacent(short src, short dest);

void write_log(char* log_msg);


unsigned int time_elapse(struct timeval start, struct timeval end);
void *check_neighbors_alive(void* arg);


unsigned char *encode_structure(short *sendIdx, short counter);

void decode_topology(char *msg, int *seqNum, short *sourceID);

void dijkstra();

void *send_neighbor_costs(void *unusedParam);
