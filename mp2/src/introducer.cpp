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


void send_to_daemon(char* ip, message_info send_msg) {
     // create new socket to send
    int sendfd; 
    struct sockaddr_in sendaddr; 

    // Creating socket file descriptor 
    if ( (sendfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
    memset(&sendaddr, 0, sizeof(sendaddr)); 
        
    // Filling server information 
    sendaddr.sin_family = AF_INET; 
    sendaddr.sin_port = htons(PORT); 
    sendaddr.sin_addr.s_addr = inet_addr(ip); 

    int n = sendto(sendfd, &send_msg, sizeof(struct message_info), 
        MSG_CONFIRM, (const struct sockaddr *) &sendaddr,  
        sizeof(sendaddr));
    //TODO recv ACK?? 
    close(sendfd);
}

int main(int argc, char *argv[]) {
    
    std::cout << "Introducer" << std::endl;
    
    while (running) {
        int sockfd; 
        struct sockaddr_in servaddr; 

        // Creating socket file descriptor 
        if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
            perror("socket creation failed"); 
            exit(EXIT_FAILURE); 
        } 
        memset(&servaddr, 0, sizeof(servaddr)); 
            
        // Filling server information 
        servaddr.sin_family = AF_INET; 
        servaddr.sin_port = htons(PORT); 
        servaddr.sin_addr.s_addr = INADDR_ANY; 
               
        socklen_t len; 
        // recv request from daemons 
        message_info recv_msg = {}; 
        int n = recvfrom(sockfd, &recv_msg, sizeof(struct message_info),  
                    MSG_WAITALL, (struct sockaddr *) &servaddr, 
                    &len); 
        if (n == -1) {
            perror("recv:");
        }

        // handle recv mesg 
        if (recv_msg.message_code == JOIN) {
            pthread_mutex_lock(&ring_lock); // lock for entire duration because position could chnage on simultaneous leave 
            // formulate msg to send to all daemons 
            message_info send_msg = {};
            send_msg.message_code = JOIN; 
            send_msg.position = ring.size(); // add to back of ring 
            send_msg.timestamp = recv_msg.timestamp;
            strncpy(send_msg.daemon_ip, recv_msg.sender_ip, 16);
            // TODO add introcuer ip (not sure we need to do this tho)
            for (daemon_info daemon : ring) {  // send new add info to all daemons 
               send_to_daemon(daemon.ip, send_msg);
            }
            // add to local ring 
            daemon_info info = {}; 
            strncpy(info.ip, recv_msg.sender_ip, 16); //TODO HEAP!! 
            info.timestamp = recv_msg.timestamp; 
            ring.push_back(info);
            pthread_mutex_unlock(&ring_lock);
        } else if (recv_msg.message_code == LEAVE) {
            // TODO leave logic copy from daemon.cpp 
        }
        
        close(sockfd); 
    }
}