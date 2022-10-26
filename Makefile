all: vec ls debugtoy manager

vec: main.c monitor_neighbors.c
	g++ -pthread -o vec_router main.c monitor_neighbors.c

ls: main.c monitor_neighbors.c
	g++ -pthread -o ls_router main.c monitor_neighbors.c

debugtoy: toy.c
	g++ -o toy toy.c
manager: manager_send.c
	gcc -o manager manager_send.c

.PHONY: clean
clean:
	rm *.o vec_router ls_router toy manager
