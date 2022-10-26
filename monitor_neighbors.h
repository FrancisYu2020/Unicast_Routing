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

extern int globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];

extern char *filename;
extern struct tableItem forwardingTable[256];

struct tableItem {
	unsigned int cost;
	int seqNum;
	short int nexthop;
	unsigned int dist;
	int isNeighbor;
};

struct LSA {
	//
};

void hackyBroadcast(const char* buf, int length);

void* announceToNeighbors(void* unusedParam);

void *listenForNeighbors(void* arg);

void print_costMatrix();

void init_cost();

void write_log(char* log_msg);

unsigned int time_elapse(struct timeval start, struct timeval end);
void *check_neighbors_alive(void* arg);

unsigned int get_cost(int source, int target);
void set_cost(int source, int target, unsigned int newCost);

unsigned char *encode_structure();

struct tableItem *decode_structure();

void dijkstra(int root);
