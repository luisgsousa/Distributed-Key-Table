/*
 *  File name: main.c
 *
 *  Author: Francisco e Luís
 *
 *  date: 2020/03
 *
 *  Description: Main
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <netdb.h>
#include <string.h>
#include <errno.h>


#include <sys/time.h> 

#define DEBUG				 1
#define MAX_INPUT_LEN        100
#define IP_LEN               17
#define PORT_LEN             6
#define MAX_RING_LENGTH		 100

#define max(A,B) ((A)>=(B)?(A):(B))


//Data structure containing information about the node's current state
typedef struct state_info
{
	int node_key;
	char node_IP[IP_LEN];
	char node_TCP[PORT_LEN];

	int succ_key;
	char succ_IP[IP_LEN];
	char succ_TCP[PORT_LEN];

	int succ2_key;
	char succ2_IP[IP_LEN];
	char succ2_TCP[PORT_LEN];

	char boot_IP[IP_LEN];
	char boot_TCP[PORT_LEN];
	int boot_key;

} state_info;

int new_node(state_info state);
state_info find_succ(state_info state);
int DistanceCompare(int key, int node_key, int succ_key);
int writeTCP(int fd, char* buff);

int main(int argc, char *argv[])
{
	char input[MAX_INPUT_LEN];
	int exit = 0;
//Variable containing the node state of the machine hosting the application
	state_info state;

//Checking to see if input is correct
	if(argc != 3){
		if(DEBUG==1)printf("Usage: ./dkt IP port\n");
		return 0;
	}


	sscanf(argv[1], "%s", state.node_IP);
	sscanf(argv[2], "%s", state.node_TCP);

	while(exit != 1)
	{
//Checking for input continuosly and calling the requested functions
		fgets(input,MAX_INPUT_LEN,stdin);
		if(sscanf(input, "new %d", &state.node_key) == 1){
			if(DEBUG==1)printf("ok %d\n", state.node_key);
			state.succ_key = state.node_key;
			state.succ2_key = state.node_key;
			strcpy(state.succ_IP, state.node_IP);
			strcpy(state.succ2_IP, state.node_IP);
			strcpy(state.succ_TCP, state.node_TCP);
			strcpy(state.succ2_TCP, state.node_TCP);
			exit = new_node(state);
		}
		else if(sscanf(input, "sentry %d %d %s %s", &state.node_key, &state.succ_key, state.succ_IP, state.succ_TCP) == 4){
			exit = new_node(state);
		}
		else if (sscanf(input, "entry %d %d %s %s", &state.node_key, &state.boot_key, state.boot_IP, state.boot_TCP) == 4){
			state = find_succ(state);
			if(state.node_key != state.succ_key)
				exit = new_node(state);
			else
				printf("key already taken by another server\n");
		}
		else if(strcmp("exit\n", input) == 0){
			exit = 1;
		}
		memset(input, 0, sizeof(input));
	}
	if(DEBUG==1)printf("Program terminated with success\n");
	return 0;
}

//Function that writes the input string using TCP
int writeTCP(int fd, char* buff){
	ssize_t nbytes,nleft,nwritten; 
	char *ptr,buffer[128];
	
	ptr=strcpy(buffer,buff); 
	nbytes=sizeof(buffer);

	nleft=nbytes; 
	while(nleft>0){
		nwritten=write(fd,ptr,nleft);
		if(nwritten<=0){
			if(DEBUG==1)printf("Error while writing on fd: %d, buffer: %s", fd, buff);
			/*error*/exit(1); 
		}
		nleft-=nwritten;
		ptr+=nwritten;
	}
	return 0;
}

//Function that locates the supposed successor of a node trying to enter the ring
state_info find_succ(state_info state){
	int fd,errcode,key;
	ssize_t n;
	socklen_t addrlen;
	struct addrinfo hints,*res;
	struct sockaddr_in addr;
	char buffer[128];

	fd=socket(AF_INET,SOCK_DGRAM,0); 
	if(fd==-1) /*error*/exit(1);

	memset(&hints,0,sizeof hints); 
	hints.ai_family=AF_INET; 
	hints.ai_socktype=SOCK_DGRAM;

	errcode=getaddrinfo(state.boot_IP,state.boot_TCP,&hints,&res); 
	if(errcode!=0) /*error*/ exit(1);

	memset(buffer, 0, sizeof(buffer));
	sprintf(buffer, "EFND %d", state.node_key);
	n=sendto(fd,buffer,128,0,res->ai_addr,res->ai_addrlen); 
	if(n==-1) /*error*/ exit(1);

	addrlen=sizeof(addr); 
	n=recvfrom(fd,buffer,128,0,(struct sockaddr*)&addr,&addrlen); 
	if(n==-1) /*error*/ exit(1);

	sscanf(buffer, "EKEY %d %d %s %s", &key, &state.succ_key, state.succ_IP, state.succ_TCP);

	freeaddrinfo(res); 
	close(fd);

	return state;
}


//Function that introduces the node to the ring and performs all node functions while it is in the network
int new_node(state_info state) {
	int fd,errcode,new_pred_fd=0,pred_fd=0,succ_fd,newfd=0,maxfd,counter,key,udp_sv_fd=0, succ_determination=0, cl_flag=0;
	ssize_t n;
	socklen_t addrlen, addrlen_udp;
	struct addrinfo hints,*res,hints_cl,*res_cl, hints_search, *res_search, hints_udp, *res_udp;
	struct sockaddr_in addr, addr_udp;
	char buffer[128];
	fd_set rfds;
	enum {idle,busy,full} server_state;
	int search_fd;
	int search_key;
	char search_IP[IP_LEN];
	char search_TCP[PORT_LEN];

	fd=socket(AF_INET,SOCK_STREAM,0); 
	if (fd==-1) exit(1); 
	succ_fd=socket(AF_INET,SOCK_STREAM,0); 
	if (succ_fd==-1) exit(1); 
	udp_sv_fd=socket(AF_INET,SOCK_DGRAM,0); //UDP socket
	if(fd==-1) /*error*/exit(1);

	memset(&hints_udp,0,sizeof hints_udp); 
	hints_udp.ai_family=AF_INET; // IPv4 
	hints_udp.ai_socktype=SOCK_DGRAM; // UDP socket 
	hints_udp.ai_flags=AI_PASSIVE;

	memset(&hints_cl,0,sizeof hints_cl); 
	hints_cl.ai_family=AF_INET; 
	hints_cl.ai_socktype=SOCK_STREAM;

	memset(&hints_search,0,sizeof hints_search); 
	hints_search.ai_family=AF_INET; 
	hints_search.ai_socktype=SOCK_STREAM;

	memset(&hints,0,sizeof hints); 
	hints.ai_family=AF_INET; 
	hints.ai_socktype=SOCK_STREAM; 
	hints.ai_flags=AI_PASSIVE;


	errcode=getaddrinfo(state.node_IP,state.node_TCP,&hints,&res); 
	if((errcode)!=0)/*error*/exit(1);
	errcode=getaddrinfo(state.node_IP,state.node_TCP,&hints_udp,&res_udp); 
	if(errcode!=0) /*error*/ exit(1);
	n=bind(udp_sv_fd,res_udp->ai_addr, res_udp->ai_addrlen); 
	if(n==-1) /*error*/ exit(1);
	n=bind(fd,res->ai_addr,res->ai_addrlen); 
	if(n==-1) /*error*/ exit(1);
	if(listen(fd,5)==-1)/*error*/exit(1);
	if(DEBUG==1)printf("TCP server started on: %s:%s\n", state.node_IP, state.node_TCP);

	//client connection to succ

	if(state.node_key != state.succ_key){
		cl_flag = 1;
		errcode=getaddrinfo(state.succ_IP,state.succ_TCP,&hints_cl,&res_cl); 
		if(errcode!=0) exit(1);

		n=connect(succ_fd,res_cl->ai_addr,res_cl->ai_addrlen); 
		if(n==-1) exit(1);
		if(DEBUG==1)printf("Connected to: %d\n", state.succ_key);

		memset(buffer, 0, sizeof(buffer));
		sprintf(buffer, "NEW %d %s %s\n", state.node_key, state.node_IP, state.node_TCP);
		writeTCP(succ_fd, buffer);
		if(DEBUG==1)printf("Sent through succ_fd: %s\n", buffer);
	}

	server_state=idle;
	while(1){
		FD_ZERO(&rfds);
		FD_SET(STDIN_FILENO,&rfds);
		FD_SET(fd,&rfds);
		maxfd=fd;
		FD_SET(udp_sv_fd,&rfds);
		maxfd=max(maxfd,udp_sv_fd);
		FD_SET(succ_fd,&rfds);
		maxfd=max(maxfd,succ_fd);
		if(pred_fd > 0){
			FD_SET(pred_fd,&rfds);
			maxfd=max(maxfd,pred_fd);
		}
		if(new_pred_fd > 0){
			FD_SET(new_pred_fd,&rfds);
			maxfd=max(maxfd,new_pred_fd);
		}
		
		counter=select(maxfd+1,&rfds,(fd_set*)NULL,(fd_set*)NULL,(struct timeval *)NULL);
		if(counter<=0)/*error*/exit(1);

		if(FD_ISSET(fd,&rfds)){
			addrlen=sizeof(addr);
			if((newfd=accept(fd,(struct sockaddr*)&addr,&addrlen))==-1) /*error*/ exit(1);
			switch(server_state){
				case idle: 
					pred_fd=newfd;
					server_state=busy;
					if(DEBUG==1)printf("New connection assigned to pred_fd\n");
					break;
				case busy:
					new_pred_fd = newfd;
					server_state=full;
					if(DEBUG==1)printf("New connection assigned to new_pred_fd\n");
					break;
				case full:
					close(newfd);
					break;
			}
		}
		if(FD_ISSET(new_pred_fd,&rfds) && (new_pred_fd != 0)){
			if((n=read(new_pred_fd,buffer,128))!=0){
				if(n==-1)/*error*/exit(1);
				if(DEBUG==1)printf("Received on new_pred_fd: %s", buffer);

				if((buffer[0] == 'K') & (buffer[1] == 'E') & (buffer[2] == 'Y')){
					if(succ_determination == 0){
						if(DEBUG==1)printf("Found key\n");
						printf("%s", buffer);
					}else{
						char aux[128] = "E";
						strcat(aux,buffer);
						if(DEBUG==1)printf("ur succ is: %s\n", aux);
						n=sendto(udp_sv_fd,aux,128,0,(struct sockaddr*)&addr_udp,addrlen_udp);
						if(n==-1)/*error*/exit(1);
						succ_determination = 0;
					}
				}
				else{
					if(state.node_key != state.succ_key){
						writeTCP(pred_fd,buffer);
						if(DEBUG==1)printf("Sent through pred_fd: %s\n", buffer);
					}else{
						sscanf(buffer, "NEW %d %s %s\n", &state.succ_key, state.succ_IP, state.succ_TCP);

						errcode=getaddrinfo(state.succ_IP,state.succ_TCP,&hints_cl,&res_cl); 
						if(errcode!=0) exit(1);
						cl_flag = 1;

						n=connect(succ_fd,res_cl->ai_addr,res_cl->ai_addrlen); 
						if(n==-1) exit(1);
						if(DEBUG==1)printf("Connected to %d\n", state.succ_key);

						writeTCP(new_pred_fd,buffer);
						if(DEBUG==1)printf("Sent through new_pred_fd: %s\n", buffer);
					}
				
					memset(buffer, 0, sizeof(buffer));
					sprintf(buffer, "SUCC %d %s %s\n", state.succ_key, state.succ_IP, state.succ_TCP);
					writeTCP(new_pred_fd,buffer);
					if(DEBUG==1)printf("Sent through new_pred_fd: %s\n", buffer);
				}
			}
			else{
				if(DEBUG==1)printf("new_pred_fd close\n");
				close(new_pred_fd);
				server_state=busy;
				new_pred_fd = 0;
			}
		}
		if(FD_ISSET(pred_fd,&rfds) && (pred_fd != 0)){
			if((n=read(pred_fd,buffer,128))!=0){
				if(n==-1)/*error*/exit(1);
				if(DEBUG==1)printf("Received on pred_fd: %s", buffer);
				if((buffer[0] == 'N') & (buffer[1] == 'E') & (buffer[2] == 'W')){
				
					sscanf(buffer, "NEW %d %s %s\n", &state.succ_key, state.succ_IP, state.succ_TCP);

					errcode=getaddrinfo(state.succ_IP,state.succ_TCP,&hints_cl,&res_cl); 
					if(errcode!=0) exit(1);
					cl_flag = 1;

					n=connect(succ_fd,res_cl->ai_addr,res_cl->ai_addrlen); 
					if(n==-1) exit(1);
					if(DEBUG==1)printf("Connected to %s:%s (%d)\n", state.succ_IP,state.succ_TCP, state.succ_key);

					writeTCP(succ_fd, "SUCCCONF\n");
					if(DEBUG==1)printf("Sent SUCCCONF through succ_fd\n");

					memset(buffer, 0, sizeof(buffer));
					sprintf(buffer, "SUCC %d %s %s\n", state.succ_key, state.succ_IP, state.succ_TCP);
					writeTCP(pred_fd, buffer);
					if(DEBUG==1)printf("Sent through pred_fd: %s\n", buffer);
				}
				else if((buffer[0] == 'F') & (buffer[1] == 'I') & (buffer[2] == 'N') & (buffer[3] == 'D')){
					
					sscanf(buffer, "FIND %d %d %s %s\n", &key, &search_key, search_IP, search_TCP);

					if(DistanceCompare(key, state.node_key, state.succ_key) == 1){

						search_fd=socket(AF_INET,SOCK_STREAM,0); 
						if (search_fd==-1) exit(1); 

						errcode=getaddrinfo(search_IP,search_TCP,&hints_search,&res_search); 
						if(errcode!=0) exit(1);

						n=connect(search_fd,res_search->ai_addr,res_search->ai_addrlen); 
						if(n==-1) exit(1);
						if(DEBUG==1)printf("Connected to %s:%s (%d)\n", search_IP, search_TCP, search_key);

						memset(buffer, 0, sizeof(buffer));
						sprintf(buffer, "KEY %d %d %s %s\n", key, state.succ_key ,state.succ_IP, state.succ_TCP);
						writeTCP(search_fd, buffer);
						if(DEBUG==1)printf("Sent through search_fd: %s\n", buffer);

						freeaddrinfo(res_search);
						close(search_fd);

					}
					else{
						writeTCP(succ_fd, buffer);
						if(DEBUG==1)printf("Sent through succ_fd: %s\n", buffer);
					}
				}
				else if(strcmp("SUCCCONF\n", buffer) == 0){
					if(DEBUG==1)printf("received SUCCCONF\n");
					memset(buffer, 0, sizeof(buffer));
					sprintf(buffer, "SUCC %d %s %s\n", state.succ_key, state.succ_IP, state.succ_TCP);
					writeTCP(pred_fd, buffer);
					if(DEBUG==1)printf("Sent through pred_fd: %s\n", buffer);
				}
			}
			else{
				if(new_pred_fd != 0){
					if(DEBUG==1)printf("new_pred_fd assigned to pred_fd, new_pred_fd now free\n");
					pred_fd = new_pred_fd;
					new_pred_fd=0;
					server_state=busy;
				}else{
					if(DEBUG==1)printf("close pred_fd\n");
					close(pred_fd);
					pred_fd=0;
					server_state=idle;
				}
			}
		}		
		if(FD_ISSET(succ_fd,&rfds)){
			if((n=read(succ_fd,buffer,128))!=0){
				if(n==-1)/*error*/exit(1);
				if(DEBUG==1)printf("Received on succ_fd: %s", buffer);
				if((buffer[0] == 'N') & (buffer[1] == 'E') & (buffer[2] == 'W')){
					state.succ2_key = state.succ_key;
					strcpy(state.succ2_IP, state.succ_IP);
					strcpy(state.succ2_TCP, state.succ_TCP);

					sscanf(buffer, "NEW %d %s %s\n", &state.succ_key, state.succ_IP, state.succ_TCP);

					if(state.node_key != state.succ_key){

						freeaddrinfo(res_cl); 
						close(succ_fd);
						if(DEBUG==1)printf("closed connection with success\n");

						succ_fd=socket(AF_INET,SOCK_STREAM,0); 
						if (succ_fd==-1) exit(1); 

						memset(&hints_cl,0,sizeof hints_cl); 
						hints_cl.ai_family=AF_INET; 
						hints_cl.ai_socktype=SOCK_STREAM;

						errcode=getaddrinfo(state.succ_IP,state.succ_TCP,&hints_cl,&res_cl); 
						if(errcode!=0) exit(1);
						cl_flag = 1;

						n=connect(succ_fd,res_cl->ai_addr,res_cl->ai_addrlen); 
						if(n==-1) exit(1);
						if(DEBUG==1)printf("Connected to %s:%s (%d)\n", state.succ_IP,state.succ_TCP, state.succ_key);

					}
					writeTCP(succ_fd,"SUCCCONF\n");
					if(DEBUG==1)printf("Sent SUCCCONF through succ_fd\n");

					memset(buffer, 0, sizeof(buffer));
					sprintf(buffer, "SUCC %d %s %s\n", state.succ_key, state.succ_IP, state.succ_TCP);
					writeTCP(pred_fd, buffer);
					if(DEBUG==1)printf("Sent through pred_fd: %s\n", buffer);
				}else{
					sscanf(buffer, "SUCC %d %s %s\n", &state.succ2_key, state.succ2_IP, state.succ2_TCP);
				}
			}
			else{
				if(DEBUG==1)printf("succ_fd close\n");
				close(succ_fd);
				freeaddrinfo(res_cl); 
				server_state=busy;

				memset(buffer, 0, sizeof(buffer));
				sprintf(buffer, "SUCC %d %s %s", state.succ2_key, state.succ2_IP, state.succ2_TCP);
				writeTCP(pred_fd, buffer);
				if(DEBUG==1)printf("Sent through pred_fd: %s\n", buffer);

				state.succ_key = state.succ2_key;
				strcpy(state.succ_IP, state.succ2_IP);
				strcpy(state.succ_TCP, state.succ2_TCP);

				succ_fd=socket(AF_INET,SOCK_STREAM,0); 
				if (succ_fd==-1) exit(1); 

				memset(&hints_cl,0,sizeof hints_cl); 
				hints_cl.ai_family=AF_INET; 
				hints_cl.ai_socktype=SOCK_STREAM;

				errcode=getaddrinfo(state.succ_IP,state.succ_TCP,&hints_cl,&res_cl); 
				if(errcode!=0) exit(1);
				cl_flag = 1;

				n=connect(succ_fd,res_cl->ai_addr,res_cl->ai_addrlen); 
				if(n==-1) exit(1);
				if(DEBUG==1)printf("Connected to %s:%s (%d)\n", state.succ_IP,state.succ_TCP, state.succ_key);

				writeTCP(succ_fd, "SUCCCONF\n");
				if(DEBUG==1)printf("Sent through succ_fd: %s\n", buffer);
			}
		}	
		if(FD_ISSET(udp_sv_fd,&rfds)){
			if(DEBUG==1)printf("udp_sv_fd active\n");
			addrlen_udp=sizeof(addr_udp); 
			n=recvfrom(udp_sv_fd,buffer,128,0,(struct sockaddr*)&addr_udp,&addrlen_udp);
			if(n==-1)/*error*/exit(1);
			sscanf(buffer, "EFND %d", &key);
			succ_determination = 1;
			if(state.node_key != state.succ_key){
				memset(buffer, 0, sizeof(buffer));
				sprintf(buffer, "FIND %d %d %s %s\n", key, state.node_key ,state.node_IP, state.node_TCP);
				writeTCP(succ_fd, buffer);
				if(DEBUG==1)printf("Sent through succ_fd: %s\n", buffer);
			}
			else{
				memset(buffer, 0, sizeof(buffer));
				sprintf(buffer, "EKEY %d %d %s %s\n", key, state.node_key ,state.node_IP, state.node_TCP);
				n=sendto(udp_sv_fd,buffer,128,0,(struct sockaddr*)&addr_udp,addrlen_udp);
				if(n==-1)/*error*/exit(1);
			}
		}
		if(FD_ISSET(STDIN_FILENO,&rfds)){
			memset(buffer, 0, sizeof(buffer));
			if((n=read(STDIN_FILENO,buffer,128))!=0){
				if(strcmp("show\n", buffer) == 0){
					printf("node: %d -> %s:%s\n", state.node_key, state.node_IP, state.node_TCP);
					printf("succ: %d -> %s:%s\n", state.succ_key, state.succ_IP, state.succ_TCP);
					printf("succ2: %d -> %s:%s\n", state.succ2_key, state.succ2_IP, state.succ2_TCP);
				}
				else if((strcmp("leave\n", buffer) == 0) || (strcmp("exit\n", buffer) == 0)){
					int cmd = (buffer[0] == 'l') ? 0 : 1;
					close(fd);
					close(pred_fd);
					close(succ_fd);
					close(udp_sv_fd);

					if(cl_flag == 1)
						freeaddrinfo(res_cl); 
					freeaddrinfo(res); 
					freeaddrinfo(res_udp);

					return cmd;
				} else if (sscanf(buffer, "find %d", &key) == 1){

					if(key == state.node_key){
						printf("KEY %d %d %s %s\n", key, state.node_key, state.node_IP, state.node_TCP);
					}
					else if(state.node_key != state.succ_key){
						memset(buffer, 0, sizeof(buffer));
						sprintf(buffer, "FIND %d %d %s %s\n", key, state.node_key ,state.node_IP, state.node_TCP);
						writeTCP(succ_fd, buffer);
						if(DEBUG==1)printf("Sent through succ_fd: %s\n", buffer);
					}
					else{
						printf("KEY %d %d %s %s\n", key, state.node_key, state.node_IP, state.node_TCP);
					}
				}
			}
		}
	}	
}

//function that calculates the key value's distance to the current node and to its successor, 
//returning 1 if the distance to the current node is bigger than the one relative to the successor and 0 if the opposite is true

int DistanceCompare(int key, int node_key, int succ_key)
{
	int node_dist = 0, succ_dist = 0;

	if(key > node_key)
		node_dist = MAX_RING_LENGTH - key + node_key;
	else
		node_dist = node_key - key;

	if(key > succ_key)
		succ_dist = MAX_RING_LENGTH - key + succ_key;
	else
		succ_dist = succ_key - key;

	if(succ_dist < node_dist)
		return 1;
	else
		return 0;
}



