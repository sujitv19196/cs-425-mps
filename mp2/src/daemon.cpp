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
// constexpr int MSG_CONFIRM = 0; // TODO Remove when movve to VMs (only a thing to fix make on mac)
 
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

pthread_mutex_t ring_lock;      // mutex to access & modify ring (there are 2 threads using it: main & child)
std::vector<daemon_info> ring;  // ring storing all daemons in order; modulo used for indexing
int running = 1;    // whether the entire system is running
// keep track of the positions of the 3 targets of the current daemon (basically next 3 in the ring)
// we are keeping track of these separate from the ring because it's easier to lookup
int targets[3] = {-1, -1, -1}; 
size_t current_pos = -1;     // Position of ourself in ring (for easy indexing)
char ip[IP_SIZE];   // IP address of ourself
char* introducer_ip = "localhost"; // TODO PLACEHOLDER

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

// Function to add daemon to ring
// Must also update targets
void add_daemon_to_ring(daemon_info daemon) {
    pthread_mutex_lock(&ring_lock);
    ring.push_back(daemon);
    targets[0] = (current_pos + 1) % ring.size();
    targets[1] = (current_pos + 2) % ring.size();
    targets[2] = (current_pos + 3) % ring.size();
    pthread_mutex_unlock(&ring_lock);
}

// Function to remove daemon from ring
// Updates current daemon's position and position of all targets
// If multiple daemons are called, it is caller's responsibility to keep track of position changes
void remove_daemon_from_ring_assist(size_t position) {
    ring.erase(ring.begin() + position);
    current_pos = position_of_daemon(ip);
    targets[0] = (current_pos + 1) % ring.size();
    targets[1] = (current_pos + 2) % ring.size();
    targets[2] = (current_pos + 3) % ring.size();
}

// Wrapper for remove daemon
// Takes ip address into account to calculate position
// Position may change during simultaneous deletes
void remove_daemon_from_ring(char ip[IP_SIZE]) {
    pthread_mutex_lock(&ring_lock);
    remove_daemon_from_ring_assist(position_of_daemon(ip));
    pthread_mutex_unlock(&ring_lock);

}

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
        message_info msg = {};
        int n = recvfrom(sockfd, &msg, sizeof(struct message_info),  
                MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
                &len); 
        printf("pinged by: %s\n", msg.sender_ip); 

        // handle Join and Leave 
        if (msg.message_code == JOIN) {
            daemon_info info = {};
            strncpy(info.ip, msg.daemon_ip, IP_SIZE);
            info.timestamp = msg.timestamp; 
            add_daemon_to_ring(info);
        } else if (msg.message_code == LEAVE) {
            remove_daemon_from_ring(msg.daemon_ip);
        }

        struct message_info send_msg; 
        send_msg.message_code = ACK; 
        sendto(sockfd, &send_msg, sizeof(struct message_info),  
            MSG_CONFIRM, (const struct sockaddr *) &cliaddr, 
                len);
        printf("ACK sent\n");  
    }
    close(sockfd);

    return 0;
}

// TODO introcuder connext function to get vector info 
// keep recv daemon-info until stop character 
void ping_introducer(char* vm_ip) {
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
    servaddr.sin_addr.s_addr = inet_addr(introducer_ip); 
    
    // set recv timeout 
    struct timeval tv;
    tv.tv_sec = 1; // timeout of 1 sec 
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    int n; 
    socklen_t len; 
    message_info send_msg = {};
    send_msg.message_code = JOIN; 
    strncpy(send_msg.sender_ip, vm_ip, IP_SIZE);
    printf("send\n");
    sendto(sockfd, &send_msg, sizeof(struct message_info), 
            MSG_CONFIRM, (const struct sockaddr *) &servaddr,  
                sizeof(servaddr)); 
    
    // recv size of ring
    size_t ring_size; 
    n = recvfrom(sockfd, &ring_size, sizeof(size_t),  
            MSG_WAITALL, (struct sockaddr *) &servaddr, 
            &len); 
    current_pos = ring_size-1;

    daemon_info daemons[ring_size];
    n = recvfrom(sockfd, &daemons, sizeof(struct daemon_info) * ring_size,  
            MSG_WAITALL, (struct sockaddr *) &servaddr, 
            &len); 
    // TODO ADD ACKS AND RETRANSMITS 
    for (int i = 0; i < ring_size; i++) {
        ring[i] = daemons[i];
    }
    printf("%zu %s\n", ring_size, daemons[0].ip);
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

// Main thread duties:
// 1. Talk to introducer to add itself to system 
// 2. Send pings 
// 3. Send failure notices (in case ping times out)

int main(int argc, char *argv[]) {
    // Parse arguments
	if (argc > 2) {
	    fprintf(stderr, "usage: daemon [-i]\n");
	    exit(1);
	}
    
    std::cout << "normal daemon" << std::endl;
    
    char* vm_ip = get_vm_ip();
    printf("%s\n", vm_ip);

    // Initialize mutex
    pthread_mutex_init(&ring_lock, NULL);

    // talk to introducer 
    // ping_introducer(vm_ip);

    // Create recv thread to recv pings and send back ACKs 
    pthread_t receive_thread; 
    pthread_create(&receive_thread, NULL, receive_pings, NULL); // TODO add args 
    
    int curr_daemon = 0; // current daemon we are pinging 
    // TODO dont start until ring is size 5  
    while (running) {  // TODO while loop to send pings, update list, handle adds/deletes, detect failiures   
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
        // servaddr.sin_addr.s_addr = inet_addr(ring[targets[curr_daemon]].ip); 
        servaddr.sin_addr.s_addr = inet_addr("localhost"); 

        
        // set recv timeout 
        struct timeval tv;
        tv.tv_sec = 1; // timeout of 1 sec 
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        int n; 
        socklen_t len; 
        message_info send_msg = {};
        send_msg.message_code = PING; 
        strncpy(send_msg.sender_ip, vm_ip, IP_SIZE);
        
        // send PING to target proc 
        sendto(sockfd, &send_msg, sizeof(struct message_info), 
            MSG_CONFIRM, (const struct sockaddr *) &servaddr,  
                sizeof(servaddr)); 
        
        // recv message from proc 
        message_info recv_msg; 
        n = recvfrom(sockfd, &recv_msg, sizeof(struct message_info),  
                    MSG_WAITALL, (struct sockaddr *) &servaddr, 
                    &len); 
        if (n == -1) {
            if ((errno== EAGAIN) || (errno == EWOULDBLOCK)) {
                // timeout
                //TODO handle timeouts/failiures 
                printf("no response\n");
            }
        }
        // printf("Server : %d\n", recv_msg.comm_type); 

        curr_daemon = (curr_daemon + 1) % 3; 
        close(sockfd); 
    }
    
    
    
    pthread_join(receive_thread, NULL);
    pthread_mutex_destroy(&ring_lock);
}

