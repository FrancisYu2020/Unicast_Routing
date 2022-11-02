from subprocess import Popen, PIPE

import subprocess
import time
import sys
import shlex
import os
import shutil
import sys

directory_results = "results/"
manager_file = "./manager/manager_send"
maketopology_file = "manager/make_topology.pl"
network_name = "enps03" #Replace this with the name of the network you see with ifconfig command in the terminal
program_name = "./ls_router" #Replace this with your program name

def initialize_results():
	if (os.path.exists(directory_results)):
		shutil.rmtree(directory_results)
	os.makedirs(directory_results)


def send(src,dest,message):
	subprocess.call([manager_file,str(src),"send",str(dest),message])

def cost(src,dest,cost):
	subprocess.call([manager_file,str(src),"cost",str(dest),str(cost)])
	subprocess.call([manager_file,str(dest),"cost",str(src),str(cost)])

def changeLink(n1,n2,state):
	if (state == "True"):
		subprocess.call(shlex.split("sudo iptables -I OUTPUT -s 10.1.1.%d -d 10.1.1.%d -j ACCEPT"%(n1,n2)))
		subprocess.call(shlex.split("sudo iptables -I OUTPUT -s 10.1.1.%d -d 10.1.1.%d -j ACCEPT"%(n2,n1)))
	else:
		subprocess.call(shlex.split("sudo iptables -D OUTPUT -s 10.1.1.%d -d 10.1.1.%d -j ACCEPT"%(n1,n2)))
		subprocess.call(shlex.split("sudo iptables -D OUTPUT -s 10.1.1.%d -d 10.1.1.%d -j ACCEPT"%(n2,n1)))

def run_test(i):

	cur_directory = "test"+str(i)+"/"
	file_log1 = open("temp_log","w+")

	subprocess.call(["perl",maketopology_file,cur_directory+"topology.txt",network_name])
	commands = open(cur_directory+"commands.txt","r+").read().split("\n")
	nodefile = open(cur_directory+"nodes.txt").read().split()
	type_nodefile = nodefile[0]
	if ("Count" in type_nodefile):
		num_nodes = int(nodefile[1])
		nodes = list(range(num_nodes))
	else:
		nodes = [int(a) for a in nodefile]
	type_costs = open(cur_directory+"costs").read()


	processes = []
	os.system("pkill -9 "+program_name)

	for nodeID in nodes:
		cur_node = str(nodeID)
		if ("fromFiles" in type_costs):
			cur_cost = cur_directory+"costs"+cur_node
		else:
			cur_cost = cur_directory+"empty"

		process = Popen([program_name, cur_node,cur_cost,directory_results+"test"+str(i)+"_log"+str(nodeID)],stdout=sys.stdout,stderr=sys.stdout)
		processes.append(process)

	time.sleep(5)
	for x in commands:
		if (x):
			# print(x)
			type_command = x.split()[0]
			if (type_command == "send"):
				source = x.split()[1]
				destination = x.split()[2]
				message = " ".join(x.split()[3:])
				send(source,destination,message)
				time.sleep(1)
			elif (type_command == "change"):
				node1 = int(x.split()[1])
				node2 = int(x.split()[2])
				state = x.split()[3]
				changeLink(node1 , node2 , state)
			elif (type_command == "sleep"):
				sleep_time = int(x.split()[1])
				# time.sleep(sleep_time)
				time.sleep(1);
			elif (type_command == "./m"):
				os.system(x)


	os.remove("temp_log")

	for p in processes:
		p.kill()
		p.wait()


if __name__ == '__main__':
	if (len(sys.argv)>1):
		program_name = "./"+sys.argv[1]
	if (len(sys.argv)> 2):
		network_name = sys.argv[2]
	output_zip = "out"
	num_tests = 8
	initialize_results()
	# run_test(6)
	for i in range(1,num_tests+1):
		run_test(i)
	if (os.path.isfile(output_zip+".zip")):
		os.remove(output_zip+".zip")
	os.system("zip -r "+output_zip+" "+directory_results)
	if (os.path.exists(directory_results)):
		shutil.rmtree(directory_results)
	os.system("sudo iptables --flush")
