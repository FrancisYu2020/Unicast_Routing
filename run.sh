#! /bin/bash

for var in 1 2 3 4 5 6 7
do
	./ls_router $var example_topology/testinitcosts$var log$var.txt &
done
