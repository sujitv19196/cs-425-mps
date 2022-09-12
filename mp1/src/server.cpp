/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <string> 
#include <iostream>

#define BACKLOG 10	 // how many pending connections queue will hold
#define PORT "4000"

std::string DEFAULT_LOGFILE = "logfiles/logfile_0.txt";
std::string GREP_OUTPUT_FILE = "temp_grep_output_file.txt";

int loop = 1; 

// sig handler that turns loop to false 
void sig_handler(int s)
{
    if (s == SIGINT) 
        loop  = 0; 
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); 

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sig_handler; 
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(loop) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // child proc 
			close(sockfd); 

            // read the grep request  
            char buffer[4096];
            memset(buffer, 0, sizeof buffer);  
			int num_read = recv(new_fd, buffer, sizeof buffer, 0);
            if (num_read == -1) {
                perror("recv"); 
                exit(1);
            }
            std::string request = buffer;
			std::cout << "Running grep command: |" << request << "|" << std::endl;

            // run grep command 
			FILE *fp = popen(request.c_str(), "r");
			memset(buffer, 0, sizeof buffer);
            std::string grep_output;
			while (fgets(buffer, 1024, fp)) {
				grep_output += buffer;
				memset(buffer, 0, sizeof buffer);
			}
			pclose(fp);

            // send grep result back to client 
			int bytes_left = grep_output.length(); 
			int total_bytes_sent = 0;
			while (bytes_left) {
				int s = send(new_fd, grep_output.c_str() + total_bytes_sent, 
								bytes_left, 0);
				if (s == -1) {
					perror("send");
					exit(1);
				}
				total_bytes_sent += s; 
				bytes_left -= s;
			}
			close(new_fd);
			printf("Sucessfully sent grep output back to client.\n");
			exit(0);
		}
		close(new_fd); 
	}
    close(sockfd);
	return 0;
}

