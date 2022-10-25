all: vec ls debugtoy

vec: main.c monitor_neighbors.c
	gcc -pthread -o vec_router main.c monitor_neighbors.c

ls: main.c monitor_neighbors.c
	gcc -pthread -o ls_router main.c monitor_neighbors.c

debugtoy: toy.c
	gcc -o toy toy.c

.PHONY: clean
clean:
	rm *.o vec_router ls_router toy
