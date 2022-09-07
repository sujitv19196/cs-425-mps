/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>

#include <vector>
#include <string> 

#define MAXBUFLEN 100

#define MAXDATASIZE 100 // max number of bytes we can get at once 

#define PORT "4000" // the port client will be connecting to 

#define NUM_VMS 2

struct thread_data {
	std::string ip; 
	std::string grep_command; 
};

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void* server_connect (void* arg) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	thread_data data = *((thread_data*)arg); 
	std::string grep = data.grep_command;
	if ((rv = getaddrinfo(data.ip.c_str() , PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return (void*) -1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return (void*) -1;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure


	int bytes_left = strlen(grep.c_str());
	int bytes_sent = 0;
	while (bytes_left) {
		int s = send(sockfd, grep.c_str() + bytes_sent, 
								bytes_left - bytes_sent, 0); 
		if (s == -1) {
			perror("send");
			return (void*) -1; 
		}
		bytes_sent += s; 
		bytes_left -= bytes_sent;
	}
	printf("sent request\n");
	
	std::string grep_return; 
	size_t buffer_size = 4096; 
	char buffer[buffer_size];
	memset(buffer, 0, buffer_size);
	while(1) {
		int num_recv = recv(sockfd, buffer, buffer_size, 0);
		if (num_recv == -1) {
	    	perror("recv");
	    	return (void*) -1;
		}	
		if (num_recv == 0) {
			break;
		}
		grep_return += buffer;
		memset(buffer, 0, buffer_size);
	}
	close(sockfd);
	char* grep_return_c = strdup(grep_return.c_str());
	return grep_return_c;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
	    fprintf(stderr,"usage: client grep_request\n");
	    exit(1);
	}

	std::string grep = argv[1];
	for (int i = 2; i < argc; i++) {
		grep += " ";
		grep += argv[i];
	}

	// TODO add list of vm ips 
	std::string server_ips[NUM_VMS]; 
	server_ips[0] = "localhost";
	server_ips[1] = "localhost";
	
	// create thread for each server we want to connect to 
	pthread_t threads[NUM_VMS];
	void* return_values[NUM_VMS];
	for (int i = 0; i < NUM_VMS; i++) {
		thread_data data;
		data.ip = server_ips[i]; 
		data.grep_command = grep; 
		pthread_create(&threads[i], NULL, server_connect, (void*)&data);
	}
	for (int i = 0; i < NUM_VMS; i++) {
		pthread_join(threads[i], &return_values[i]);
		printf("%s\n", (char*)return_values[0]);
	}

	for (int i = 0; i < NUM_VMS; i++) {
		free(return_values[i]);
	}
	return 0;
}

