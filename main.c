#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/time.h>

#include "monitor_neighbors.h"


// void listenForNeighbors();
// void* announceToNeighbors(void* unusedParam);
// struct ForwardingTable

int globalMyID = 0;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
struct sockaddr_in globalNodeAddrs[256];

char *filename;
struct tableItem forwardingTable[256];


int main(int argc, char** argv)
{
  // printf("%s\n", "debug start");
  // fflush(stdin);

	if(argc != 4)
	{
		fprintf(stderr, "Usage: %s mynodeid initialcostsfile logfile\n\n", argv[0]);
		exit(1);
	}
  // printf("%s\n", "debug");

  //TODO: create the log file
  filename = argv[3];
  FILE *logFile = fopen(filename, "w+");
  fclose(logFile);

	//initialization: get this process's node ID, record what time it is,
	//and set up our sockaddr_in's for sending to the other nodes.
	globalMyID = atoi(argv[1]);
	int i;
	for(i=0;i<256;i++)
	{
    // printf("%s\n", "debug loop");

		gettimeofday(&globalLastHeartbeat[i], 0);

		char tempaddr[100];
		sprintf(tempaddr, "10.1.1.%d", i);
		memset(&globalNodeAddrs[i], 0, sizeof(globalNodeAddrs[i]));
		globalNodeAddrs[i].sin_family = AF_INET;
		globalNodeAddrs[i].sin_port = htons(7777);
		inet_pton(AF_INET, tempaddr, &globalNodeAddrs[i].sin_addr);
	}

  // printf("%s\n", "debug");
  //TODO: init the cost matrix
  init_cost();
  // print_costMatrix();

	//TODO: read and parse initial costs file. default to cost 1 if no entry for a node. file may be empty.
  // printf("%s\n", "debug!!!");
  for (int i = 0; i < 256; i ++) {
    forwardingTable[i].seqNum = 0;
    forwardingTable[i].cost = 1;
    forwardingTable[i].nexthop = -1;
    forwardingTable[i].dist = -1;
    forwardingTable[i].isNeighbor = 0;
  }

  FILE* initialCostsFile = fopen(argv[2], "r");
  if (initialCostsFile == NULL)
  {
    fprintf(stderr, "initial cost file: %s not found!\n\n", argv[2]);
    exit(1);
  }
  char* line = NULL;
  size_t len = 0;
  ssize_t nread;

  while ((nread = getline(&line, &len, initialCostsFile)) != -1) {
    if (line == NULL || (strlen(line) == 0) || (isdigit(*line) == 0)) break;
    char *cost_chr = strchr(line, ' ');
    *cost_chr = '\0';
    cost_chr++;
    int nodeID = atoi(line);
    int cost = atoi(cost_chr);
    forwardingTable[nodeID].cost = cost;
  }
  fclose(initialCostsFile);

	//socket() and bind() our socket. We will do all sendto()ing and recvfrom()ing on this one.
	if((globalSocketUDP=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		exit(1);
	}
	char myAddr[100];
	struct sockaddr_in bindAddr;
	sprintf(myAddr, "10.1.1.%d", globalMyID);
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(7777);
	inet_pton(AF_INET, myAddr, &bindAddr.sin_addr);
	if(bind(globalSocketUDP, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		close(globalSocketUDP);
		exit(1);
	}

  printf("%s\n", "debug after UDP");



	//start threads... feel free to add your own, and to remove the provided ones.
	pthread_t announcerThread, messageReceiverThread, monitorThread;
  pthread_create(&announcerThread, 0, announceToNeighbors, (void*)0);
  pthread_create(&messageReceiverThread, 0, listenForNeighbors, (void*)0);
	pthread_create(&monitorThread, 0, check_neighbors_alive, (void*)0);

  printf("%s\n", "debug after pthread");


	//good luck, have fun!
	// listenForNeighbors();
  pthread_join(announcerThread, NULL);
  pthread_join(messageReceiverThread, NULL);
  pthread_join(monitorThread, NULL);
  printf("%s\n", "debug after listenForNeighbors");


}
