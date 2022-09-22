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
// constexpr int INTRODUCER_PORT = 9080;
// constexpr int MSG_CONFIRM = 0;

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
    int timestamp; // -1 means introducer is done sending ring info 
};

pthread_mutex_t ring_lock;
std::vector<daemon_info> ring;  // ring storing all daemons in order; modulo used for indexing
int running = 1;    // whether the entire system is running


// Function to compare two IP addresses
bool compare_ip(char* ip1, char* ip2) {
    return std::string(ip1) == std::string(ip2);
}

// Get the position (index) of a daemon in the vector
int position_of_daemon(char ip[IP_SIZE]) {
    for (size_t i = 0; i < ring.size(); i++) {
        if (compare_ip(ring[i].ip, ip)) {
            return i;
        }
    }
    return -1;
}

// Function to remove daemon from ring
// Updates current daemon's position and position of all targets
// If multiple daemons are called, it is caller's responsibility to keep track of position changes
void remove_daemon_from_ring_assist(size_t position) {
    ring.erase(ring.begin() + position);
}

// Wrapper for remove daemon
// Takes ip address into account to calculate position
// Position may change during simultaneous deletes
void remove_daemon_from_ring(char ip[IP_SIZE]) {
    pthread_mutex_lock(&ring_lock);
    remove_daemon_from_ring_assist(position_of_daemon(ip));
    pthread_mutex_unlock(&ring_lock);
}

void send_ring_to_new_daemon(int new_daemon_fd, sockaddr_in cliaddr) {
    // send size of ring 
    size_t ring_size = ring.size();
    int n = sendto(new_daemon_fd, &ring_size, sizeof(size_t), 
                    MSG_CONFIRM, (const struct sockaddr *) &cliaddr,  
                    sizeof(cliaddr));
    
    daemon_info daemons[ring_size];
    for (int i = 0; i < ring.size(); i++) {
        daemons[i] = ring[i];
    }
    n = sendto(new_daemon_fd, daemons, sizeof(struct daemon_info) * ring_size, 
                MSG_CONFIRM, (const struct sockaddr *) &cliaddr,  
                sizeof(cliaddr));
    // TODO check for sender error
}

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

void add_daemon_to_ring(message_info recv_msg, int new_daemon_fd, sockaddr_in cliaddr) {
    pthread_mutex_lock(&ring_lock); // lock for entire duration because position could chnage on simultaneous leave 
    // formulate msg to send to all daemons 
    message_info send_msg = {};
    send_msg.message_code = JOIN; 
    send_msg.position = ring.size(); // add to back of ring 
    send_msg.timestamp = recv_msg.timestamp;
    strncpy(send_msg.daemon_ip, recv_msg.sender_ip, 16);
    for (daemon_info daemon : ring) {  // send new add info to all daemons 
        send_to_daemon(daemon.ip, send_msg);
    }
    // add to local ring 
    daemon_info info = {}; 
    strncpy(info.ip, recv_msg.sender_ip, 16); 
    info.timestamp = recv_msg.timestamp; 
    ring.push_back(info);

    // send entire ring to new daemon 
    send_ring_to_new_daemon(new_daemon_fd, cliaddr);
    pthread_mutex_unlock(&ring_lock);
}

int main(int argc, char *argv[]) {
    
    std::cout << "Introducer" << std::endl;
    int sockfd; 
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
        message_info recv_msg = {};
        int n = recvfrom(sockfd, &recv_msg, sizeof(struct message_info),  
                    MSG_WAITALL, (struct sockaddr *) &cliaddr, 
                    &len); 
        if (n == -1) {
            perror("recv:");
        }
        // handle recv mesg 
        if (recv_msg.message_code == JOIN) {
            printf("JOIN RECV\n");
            add_daemon_to_ring(recv_msg, sockfd, cliaddr);
        } else if (recv_msg.message_code == LEAVE) {
            remove_daemon_from_ring(recv_msg.sender_ip);
        }
    }
    close(sockfd); 
}