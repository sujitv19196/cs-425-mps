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
#include<signal.h>

#include <vector>
#include <string> 
#include <iostream>
#include <fstream>
#include <chrono>
#include <mutex>
#include <thread>

// ===========================================================================================================
// Global Variables
// ===========================================================================================================

// #define NUM_VMS 5
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
};

// Structure of daemon info stored in ring
struct daemon_info {
    char ip[IP_SIZE];
    int timestamp;
};

pthread_mutex_t ring_lock;      // mutex to access & modify ring (there are 2 threads using it: main & child)
std::vector<daemon_info> ring;  // ring storing all daemons in order; modulo used for indexing
int running = 1;    // whether the entire system is running
pthread_cond_t g5_cv;   // begin pinging once ring contains more than 5 daemons
// keep track of the positions of the 3 targets of the current daemon (basically next 3 in the ring)
// we are keeping track of these separate from the ring because it's easier to lookup
int targets[3] = {-1, -1, -1}; 
size_t current_pos = -1;     // Position of ourself in ring (for easy indexing)
char my_ip[IP_SIZE];   // IP address of ourself
char introducer_ip[IP_SIZE] = "172.22.157.36";

// ===========================================================================================================
// Helper Functions
// ===========================================================================================================

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

// Function to add daemon to ring
// Must also update targets
void add_daemon_to_ring(daemon_info daemon) {
    pthread_mutex_lock(&ring_lock);
    ring.push_back(daemon);
    targets[0] = (current_pos + 1) % ring.size();
    targets[1] = (current_pos + 2) % ring.size();
    targets[2] = (current_pos + 3) % ring.size();
    if (ring.size() > 5) {
        pthread_cond_signal(&g5_cv);    // signal that pinging may begin
    }
    printf("added %s to ring\n", daemon.ip);
    pthread_mutex_unlock(&ring_lock);
}

// Send leave message to all other daemons
int send_leave(char leaving_ip[IP_SIZE]) {
    // send leave message
    pthread_mutex_lock(&ring_lock);
    for (daemon_info d: ring) { // send to each daemon expect myself and the leaving daemon
        if (!compare_ip(d.ip, leaving_ip) && !compare_ip(d.ip, my_ip)) {
            struct message_info send_msg; 
            send_msg.message_code = LEAVE; 
            send_msg.timestamp = time(NULL);
            strncpy(send_msg.sender_ip, my_ip, IP_SIZE);
            strncpy(send_msg.daemon_ip, leaving_ip, IP_SIZE);

            send_message(d.ip, &send_msg, sizeof(message_info), CHILD_PORT);
            printf("LEAVE notice sent to IP %s.\n", d.ip);
        }
    }
    pthread_mutex_unlock(&ring_lock);

    return 0;
}

// Function to remove daemon from ring
// Updates current daemon's position and position of all targets
// If multiple daemons are called, it is caller's responsibility to keep track of position changes
void remove_daemon_from_ring_assist(size_t position) {
    pthread_mutex_lock(&ring_lock);
    ring.erase(ring.begin() + position);
    current_pos = position_of_daemon(my_ip);
    targets[0] = (current_pos + 1) % ring.size();
    targets[1] = (current_pos + 2) % ring.size();
    targets[2] = (current_pos + 3) % ring.size();
    pthread_mutex_unlock(&ring_lock); // TODO LOOK 
}

// Wrapper for remove daemon
// Takes ip address into account to calculate position
// Position may change during simultaneous deletes
void remove_daemon_from_ring(char ip[IP_SIZE]) {
    remove_daemon_from_ring_assist(position_of_daemon(ip));
    printf("removed %s from ring\n", ip);
}


// Handle ctrl+Z signal (server gracefully quitting)
void sig_handler(int signum){    
    std::cout << "Signal handler entered." << std::endl;

    if (signum == SIGTSTP) {
        printf("SIGTSTP pressed. Attempting to leave gracefully ...\n");

        // send leave notification to all daemons
        send_leave(my_ip);

        // exit gracefully
        printf("Send leave notification to other daemons. Exiting program.\n");
        exit(0);
  }
  else if (signum == SIGQUIT) {
    printf("Quit!!!");
    exit(0);
  }
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
    servaddr.sin_port = htons(CHILD_PORT); 
        
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
            sizeof(servaddr)) < 0 ) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
        
    socklen_t len = sizeof(cliaddr);  //len is value/result 
    while(running) {
        // Recieve the message
        // printf("waiting for ping\n");
        message_info msg = {};
        int n = recvfrom(sockfd, &msg, sizeof(struct message_info),  
                MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
                &len); 
        // printf("pinged by: %s\n", msg.sender_ip); 

        // handle Join and Leave 
        if (msg.message_code == JOIN) {
            daemon_info info = {};
            strncpy(info.ip, msg.daemon_ip, IP_SIZE);
            info.timestamp = msg.timestamp; 
            add_daemon_to_ring(info);
        } else if (msg.message_code == LEAVE) {
            remove_daemon_from_ring(msg.daemon_ip);
        } else {
            // Acknowledge receipt of ping (must send to MAIN_PORT)
            struct message_info send_msg; 
            send_msg.message_code = ACK; 
            strncpy(send_msg.sender_ip, my_ip, IP_SIZE);
            send_message(msg.sender_ip, &send_msg, sizeof(message_info), MAIN_PORT);
            printf("ACK sent\n");  
        }   
    }
    close(sockfd);

    return 0;
}

// ==================================
// Introducer (called by main thread)
// ==================================

// pings introdcuer to get the entire ring and its position in the ring
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
    servaddr.sin_port = htons(INTRODUCER_PORT); 
    servaddr.sin_addr.s_addr = inet_addr(introducer_ip); 
    
    //TODO add timeout and retrasmit on introducer

    int n; 
    socklen_t len; 
    message_info send_msg = {};
    send_msg.message_code = JOIN; 
    strncpy(send_msg.sender_ip, vm_ip, IP_SIZE);
    printf("Sending message to introducer.\n");
    send_message(introducer_ip, &send_msg, sizeof(message_info), INTRODUCER_PORT);

    // recv size of ring
    size_t ring_size; 
    n = recvfrom(sockfd, &ring_size, sizeof(size_t),  
            MSG_WAITALL, (struct sockaddr *) &servaddr, 
            &len); 
    
    // update this VMs current pos 
    current_pos = ring_size-1;

    daemon_info daemons[ring_size];
    n = recvfrom(sockfd, &daemons, sizeof(struct daemon_info) * ring_size,  
            MSG_WAITALL, (struct sockaddr *) &servaddr, 
            &len); 
    // TODO ADD ACKS AND RETRANSMITS 
    for (int i = 0; i < ring_size; i++) {
        add_daemon_to_ring(daemons[i]);
    }
    // printf("%zu %s\n", ring_size, daemons[0].ip);
    close(sockfd);
}

// ===========================================================================================================
// Main Thread
// ===========================================================================================================
// Main thread duties:
// 1. Talk to introducer to add itself to system 
// 2. Send pings (and acknowledge results)
// 3. Send failure notices (in case ping times out)

int main(int argc, char *argv[]) {
    // Set up signal handling
    signal(SIGTSTP, sig_handler);
    signal(SIGQUIT, sig_handler);

    // Parse arguments
	if (argc > 2) {
	    fprintf(stderr, "usage: daemon [-i]\n");
	    exit(1);
	}
    
    std::cout << "Creating normal daemon (non-introducer)." << std::endl;
    
    // Get the ip of yourself
    strncpy(my_ip, get_vm_ip(), IP_SIZE);
    printf("%s\n", my_ip);

    // Initialize mutex
    pthread_mutex_init(&ring_lock, NULL);
    pthread_cond_init(&g5_cv, NULL);

    // talk to introducer 
    printf("Pinging introducer ...\n");
    ping_introducer(my_ip);
    printf("Successfully added to system!\n");

    // Create recv thread to recv pings and send back ACKs 
    pthread_t receive_thread; 
    pthread_create(&receive_thread, NULL, receive_pings, NULL); // TODO add args 
        
    // Don't start until ring size is greater than 5.
    
    pthread_mutex_lock(&ring_lock);
    while (ring.size() < 5) {
        printf("System currently has %zu daemons.\n Cannot proceed until 5 daemons exist.", ring.size());
        pthread_cond_wait(&g5_cv, &ring_lock);
    }
    pthread_mutex_unlock(&ring_lock);
    printf("Ring now has %zu elements. Beginning pinging from main thread.\n", ring.size());

    int curr_daemon = 0; // current daemon we are pinging 

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
        servaddr.sin_port = htons(MAIN_PORT); 
        servaddr.sin_addr.s_addr = inet_addr(ring[targets[curr_daemon]].ip); 
        // servaddr.sin_addr.s_addr = inet_addr("localhost"); 

        // set recv timeout 
        struct timeval tv;
        tv.tv_sec = 1; // timeout of 1 sec 
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        int n; 
        socklen_t len; 
        message_info send_msg = {};
        send_msg.message_code = PING; 
        send_msg.timestamp = time(NULL);
        strncpy(send_msg.sender_ip, my_ip, IP_SIZE);
        
        // send PING to target proc (must be on CHILD_PORT)
        send_message(ring[targets[curr_daemon]].ip, &send_msg, sizeof(message_info), CHILD_PORT);
        
        // recv message from proc 
        message_info recv_msg; 
        n = recvfrom(sockfd, &recv_msg, sizeof(struct message_info),  
                    MSG_WAITALL, (struct sockaddr *) &servaddr, 
                    &len); 
        if (n == -1) {
            if ((errno== EAGAIN) || (errno == EWOULDBLOCK)) {
                // TIMEOUT!
                // Must remove the target daemon from ring
                // Then send fail message to every other daemon to do the same.
                printf("Ping to daemon with ip %s timed out. Sending out failure notice.\n", ring[targets[curr_daemon]].ip);
                char leaving_ip[IP_SIZE];
                strncpy(leaving_ip, ring[targets[curr_daemon]].ip, IP_SIZE); 
                send_leave(leaving_ip);
                remove_daemon_from_ring(leaving_ip);
            }
        }
        // printf("Server : %d\n", recv_msg.comm_type); 

        curr_daemon = (curr_daemon + 1) % 3; 
        close(sockfd); 

        // stop pinging if ring < size 5 
        pthread_mutex_lock(&ring_lock);
        while (ring.size() < 5) {
            pthread_cond_wait(&g5_cv, &ring_lock);
        }
        pthread_mutex_unlock(&ring_lock);
    }
    
    pthread_join(receive_thread, NULL);
    pthread_mutex_destroy(&ring_lock);
    pthread_cond_destroy(&g5_cv);
}

