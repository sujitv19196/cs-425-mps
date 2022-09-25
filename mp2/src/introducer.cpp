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
#include <thread>

constexpr int MAIN_PORT = 8080;
constexpr int CHILD_PORT = 8081;
constexpr int INTRODUCER_PORT = 8002; 
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

// Send a message to a specific ip address
int send_message(char dest_ip[16], void* message, size_t message_len, int port) {
    int sockfd;
    struct sockaddr_in remote = {0};
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("Leave sending: socket creation failed!"); 
        exit(EXIT_FAILURE); 
    } 
    remote.sin_addr.s_addr = inet_addr(dest_ip); 
    remote.sin_family    = AF_INET; // IPv4 
    remote.sin_port = htons(port); 
    connect(sockfd, (struct sockaddr *)&remote, sizeof(struct sockaddr_in));

    send(sockfd, message, message_len, 0);
    close(sockfd);
    return 0;
}

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

// gets this vms ip addr 
char* get_vm_ip() {
    // get VM ip (adapted from https://www.geeksforgeeks.org/c-program-display-hostname-ip-address/)
    char hostbuffer[256];
    char *vm_ip;
    struct hostent *host_entry;
    int hostname;
    // To retrieve hostname
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    // To retrieve host information
    host_entry = gethostbyname(hostbuffer);
    // To convert an Internet network
    // address into ASCII string
    return inet_ntoa(*((struct in_addr*)
                           host_entry->h_addr_list[0]));
}

// Remove daemon with specified ip from ring
void remove_daemon_from_ring(char ip[IP_SIZE]) {
    pthread_mutex_lock(&ring_lock);
    ring.erase(ring.begin() + position_of_daemon(ip));
    printf("removed %s from ring\n", ip);
    pthread_mutex_unlock(&ring_lock);
}

void send_ring_to_new_daemon(char* new_daemon_ip) {
    // send size of ring 
    size_t ring_size = ring.size();
    send_message(new_daemon_ip, &ring_size, sizeof(size_t), INTRODUCER_PORT);
    // send contents of ring
    daemon_info daemons[ring_size];
    for (int i = 0; i < ring.size(); i++) {
        daemons[i] = ring[i];
    }
    send_message(new_daemon_ip, daemons, sizeof(daemon_info) * ring_size, INTRODUCER_PORT);
}

void introducer_handle_new_daemon(message_info recv_msg) {
    pthread_mutex_lock(&ring_lock); // lock for entire duration because position could chnage on simultaneous leave 
    
    // formulate msg to send to all daemons 
    message_info send_msg = {};
    send_msg.message_code = JOIN; 
    send_msg.position = ring.size(); // add to back of ring 
    send_msg.timestamp = recv_msg.timestamp;
    strncpy(send_msg.daemon_ip, recv_msg.sender_ip, 16);
    for (daemon_info daemon : ring) {  // send new add info to all daemons 
        send_message(daemon.ip, &send_msg, sizeof(message_info), CHILD_PORT);
    }

    // add to local ring 
    daemon_info info = {}; 
    strncpy(info.ip, recv_msg.sender_ip, 16); 
    info.timestamp = recv_msg.timestamp; 
    ring.push_back(info);

    // send entire ring to new daemon 
    send_ring_to_new_daemon(recv_msg.sender_ip);
    pthread_mutex_unlock(&ring_lock);
}

// ===========================================================================================================
// Child Thread Functions
// ===========================================================================================================
// Child thread duties:
// 1. Receive pings and send acknowledgements
// 2. Receive joins (and modify ring)
// 3. Receive leaves (and modify ring)

void* receive_pings (void* args) {
    // UDP server addapted from https://www.geeksforgeeks.org/udp-server-client-implementation-c/
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
    servaddr.sin_port = htons(MAIN_PORT); 
        
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
            sizeof(servaddr)) < 0 ) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
        
    socklen_t len = sizeof(cliaddr);  //len is value/result 
    while(running) {
        // Recieve the message
        printf("waiting for ping\n");
        message_info msg = {};
        int n = recvfrom(sockfd, &msg, sizeof(struct message_info),  
                MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
                &len); 
        printf("pinged by: %s\n", msg.sender_ip); 

        //TODO might have to handle ADD in the futrue? although atm all adds happen here 
        if (msg.message_code == LEAVE) {
            remove_daemon_from_ring(msg.daemon_ip);
        } else if (msg.message_code == PING) {
            struct message_info send_msg; 
            send_msg.message_code = ACK; 
            sendto(sockfd, &send_msg, sizeof(struct message_info),  
                MSG_CONFIRM, (const struct sockaddr *) &cliaddr, 
                    len);
            // printf("ACK sent\n");  
        }
    }
    close(sockfd);

    return 0;
}

int main(int argc, char *argv[]) {
    
    std::cout << "Introducer" << std::endl;
    int sockfd; 
    struct sockaddr_in servaddr, cliaddr; 
    
    // Initialize mutex
    pthread_mutex_init(&ring_lock, NULL);

    // // add the introducer to the ring 
    // pthread_mutex_lock(&ring_lock);
    // daemon_info introducer = {};
    // strncpy(introducer.ip, get_vm_ip(), IP_SIZE);
    // //TODO timestamp 
    // ring.push_back(introducer);
    // pthread_mutex_unlock(&ring_lock);

    // Create recv thread to recv pings and send back ACKs 
    pthread_t receive_thread; 
    pthread_create(&receive_thread, NULL, receive_pings, NULL); 

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
    servaddr.sin_port = htons(INTRODUCER_PORT); 
        
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
            sizeof(servaddr)) < 0 ) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
        
    socklen_t len = sizeof(cliaddr);  //len is value/result 
    while(running) {
        printf("waiting for new joins\n");
        message_info recv_msg = {};
        int n = recvfrom(sockfd, &recv_msg, sizeof(struct message_info),  
                    MSG_WAITALL, (struct sockaddr *) &cliaddr, 
                    &len); 
        if (n == -1) {
            perror("recv:");
        }
        // handle recv mesg 
        if (recv_msg.message_code == JOIN) {
            printf("JOIN RECVd\n");
            introducer_handle_new_daemon(recv_msg);
        }
    }
    close(sockfd); 
    pthread_join(receive_thread, NULL);
    pthread_mutex_destroy(&ring_lock);

}
