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
#include <arpa/inet.h> 
#include <netinet/in.h> 

#include <vector>
#include <string> 
#include <iostream>
#include <fstream>

// #define NUM_VMS 5
#define PORT   8080 
#define MSG_CONFIRM 0

#define PING 0 

int INTRODUCER_IP = 100;
int running = 1; 

struct daemon_info {
    int timestamp;
    char* ip;
    // add fields here if necessary
};

struct communication_data {
	char comm_type; 
	int from; 
    // add fields here if necessary
};

// TODO Thread to receive pings and send ACKS
void* recv_pings (void* args) {
    // UDP server addapted from https://www.geeksforgeeks.org/udp-server-client-implementation-c/
    int sockfd; 
    char buffer[4096];
    struct sockaddr_in servaddr, cliaddr; 
        
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
        
    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 
        
    // Filling server information 
    servaddr.sin_family    = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(PORT); 
        
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
            sizeof(servaddr)) < 0 ) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
        
    socklen_t len = sizeof(cliaddr);  //len is value/result 
    char* ack = "ACK";
    while(running) {
        printf("waiting for ping\n");
        memset(buffer, 0, sizeof buffer);
        int n = recvfrom(sockfd, (char *)buffer, sizeof buffer,  
                MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
                &len); 
        buffer[n] = '\0'; 
        printf("pinged by: %s\n", buffer); 
        sendto(sockfd, (const char *)ack, strlen(ack),  
            MSG_CONFIRM, (const struct sockaddr *) &cliaddr, 
                len); 
        printf("ACK sent\n");  
    }
    close(sockfd);
}

std::vector<daemon_info> daemon_list;

int main(int argc, char *argv[]) {
	if (argc > 2) {
	    fprintf(stderr, "usage: daemon [-i]\n");
	    exit(1);
	}

    if (argc == 2) {
        std::cout << "Introducer" << std::endl;
    } else {
        std::cout << "normal daemon" << std::endl;
    }

     // create recv thread to recv pings and send back ACKs 
    pthread_t recv_thraed; 
    pthread_create(&recv_thraed, NULL, recv_pings, NULL); // TODO add args 
    
    // TODO while loop to send pings, update list, handle adds/deletes, detect failiures   
   int curr_daemon = 0; 
    while (running) {
        int sockfd; 
        char buffer[4096]; 
        struct sockaddr_in     servaddr; 

        // Creating socket file descriptor 
        if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
            perror("socket creation failed"); 
            exit(EXIT_FAILURE); 
        } 
        memset(&servaddr, 0, sizeof(servaddr)); 
            
        // Filling server information 
        servaddr.sin_family = AF_INET; 
        servaddr.sin_port = htons(PORT); 
        servaddr.sin_addr.s_addr = inet_addr(daemon_list[curr_daemon].ip); 
            
        int n; 
        socklen_t len; 
        communication_data msg;
        msg.from = -1; // TODO placeholder 
        sendto(sockfd, &msg, sizeof(struct communication_data), 
            NULL, (const struct sockaddr *) &servaddr,  
                sizeof(servaddr)); 
        
        communication_data recv; 
        n = recvfrom(sockfd, &recv, sizeof(struct communication_data),  
                    NULL, (struct sockaddr *) &servaddr, 
                    &len); // TODO add timeout 
        buffer[n] = '\0'; 
        printf("Server : %s\n", buffer); 
        close(sockfd); 
    }
    
    
    
    pthread_join(recv_thraed, NULL);
}

