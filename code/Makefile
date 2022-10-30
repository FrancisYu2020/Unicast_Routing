all: ls manager

ls: main.c monitor_neighbors.c
	g++ -pthread -o ls_router main.c monitor_neighbors.c

manager: manager_send.c
	gcc -o manager manager_send.c

.PHONY: clean
clean:
	rm *.o ls_router manager
