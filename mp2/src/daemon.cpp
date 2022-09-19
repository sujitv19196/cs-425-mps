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
#include <iostream>
#include <fstream>

#define NUM_VMS 5

int INTRODUCER_IP = 100;

struct daemon_info {
    int timestamp;
    int ip;
    // add fields here if necessary
};

struct communication_data {
	char comm_type; 
	int ip1; 
    int ip2;
    // add fields here if necessary
};

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
}

