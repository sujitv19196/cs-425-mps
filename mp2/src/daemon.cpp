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
#include <pthread.h>

#include <vector>
#include <string> 
#include <iostream>
#include <fstream>
#include <chrono>
#include <mutex>
#include <thread>

// #define NUM_VMS 5
constexpr int PORT = 8080;
constexpr int MSG_CONFIRM = 0;

// Message codes
constexpr char PING = 0;
constexpr char ACK = 1;
constexpr char JOIN = 2;
constexpr char LEAVE = 3;

// Other consts
constexpr size_t IP_SIZE = 16;

// Structure of messages sent by daemon
struct message_info {
    // general info
    char message_code;
    time_t timestamp;
    char sender_ip[IP_SIZE];

    // join & leave
    char daemon_ip[IP_SIZE];
    size_t position;
};

// Structure of daemon info stored in ring
struct daemon_info {
    char ip[IP_SIZE];
    int timestamp;
};

pthread_mutex_t ring_lock;
std::vector<daemon_info> ring;  // ring storing all daemons in order; modulo used for indexing
int running = 1;    // whether the entire system is running

// Function to compare two IP addresses
bool compare_ip(char* ip1, char* ip2) {
    return std::string(ip1) == std::string(ip2);
}

// pthread_mutex_init()

// ptrhead_mutex_lock();
// // modify vectori here
// pthread_mutex_unlcok();

// Child thread duties:
// 1. Receive pings and send acknowledgements
// 2. Receive joins (and modify ring)
// 3. Receive leaves (and modify ring)
void* receive_pings (void* args) {
    // UDP server addapted from https://www.geeksforgeeks.org/udp-server-client-implementation-c/
    int sockfd; 
    struct message_info msg;
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
    while(running) {
        printf("waiting for ping\n");
        memset(&msg, 0, sizeof(struct message_info));
        int n = recvfrom(sockfd, &msg, sizeof(struct message_info),  
                MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
                &len); 
        printf("pinged by: %d\n", msg.sender_ip); 

        struct message_info send_msg; 
        send_msg.message_code = ACK; 
        sendto(sockfd, &send_msg, sizeof(struct message_info),  
            MSG_CONFIRM, (const struct sockaddr *) &cliaddr, 
                len); 
        printf("ACK sent\n");  
    }
    close(sockfd);
}

// Main thread duties:
// 1. Send pings
// 2. Send failure notices (in case ping times out)
// 3. Send join notices (in case of introducer)

int main(int argc, char *argv[]) {
    // Parse arguments
	if (argc > 2) {
	    fprintf(stderr, "usage: daemon [-i]\n");
	    exit(1);
	}
    if (argc == 2) {
        std::cout << "Introducer" << std::endl;
    } else {
        std::cout << "normal daemon" << std::endl;
    }

    // Initialize mutex
    pthread_mutex_init(&ring_lock, NULL);

    // Create recv thread to recv pings and send back ACKs 
    pthread_t receive_thread; 
    pthread_create(&receive_thread, NULL, receive_pings, NULL); // TODO add args 
    
    // TODO while loop to send pings, update list, handle adds/deletes, detect failiures   
   int curr_daemon = 0; 
    while (running) {
        int sockfd; 
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
        servaddr.sin_addr.s_addr = inet_addr("localhost"); 
        // servaddr.sin_addr.s_addr = inet_addr(daemon_list[curr_daemon].ip); 
        // set recv timeout 
        struct timeval tv;
        tv.tv_sec = 1; // timeout of 1 sec 
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        int n; 
        socklen_t len; 
        message_info send_msg;
        send_msg.message_code = PING; 
        send_msg.sender_ip[0] = 'g'; // TODO placeholder 
        
        // send PING to target proc 
        sendto(sockfd, &send_msg, sizeof(struct message_info), 
            MSG_CONFIRM, (const struct sockaddr *) &servaddr,  
                sizeof(servaddr)); 
        
        // recv message from proc 
        message_info recv_msg; 
        n = recvfrom(sockfd, &recv_msg, sizeof(struct message_info),  
                    MSG_WAITALL, (struct sockaddr *) &servaddr, 
                    &len); // TODO add timeout 
        if (n == -1) {
            if ((errno== EAGAIN) || (errno == EWOULDBLOCK)) {
                // timeout or leave 
                printf("no response\n");
            }
        }
        // printf("Server : %d\n", recv_msg.comm_type); 

        //TODO increment ring 
        close(sockfd); 
    }
    
    
    
    pthread_join(receive_thread, NULL);
}

