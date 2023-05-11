/* Q7. [9 marks]
A port scanner is a program that tries to find out which ports have servers
actively listening on them.  A simple way to do this is to iterate over a
set of ports, trying to establish a connection on each port.

Complete the C program below to implement a port scanner that is looking
for a web server listening on ports in the range LOWER to UPPER. It takes a
server address as a command line argument and attempts to determine whether 
the server listening on one or more of ports in range LOWER to UPPER is a 
web server by sending a "GET" request, and checking if a response begins 
with "HTTP". (Note that the "GET" request message is already set up for you.)

The program will print the following messages in the appropriate
circumstances. In each case "x" is the port number.
 - Port x answered with HTTP
 - Port x is not a web server
 - Port x is not responding
 - Port x is not listening
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define BLOCK_SIZE 128

// Set a small range for testing
#define LOWER 8080
#define UPPER 8090

int main(int argc, char* argv[]) {
    int i;
    int soc;
    char buf[BLOCK_SIZE];
    char buf2[BLOCK_SIZE];
    struct hostent *hp;
    struct sockaddr_in peer;

    // Create an HTTP request message
    strcpy(buf, "GET /index.html HTTP/1.0");
    peer.sin_family = AF_INET;

    /* fill in peer address */
    hp = gethostbyname(argv[1]);
    if ( hp == NULL ) {
        fprintf(stderr, "%s: %s unknown host\n",
                argv[0], argv[1]);
        exit(1);
    }

    peer.sin_addr = *((struct in_addr *)hp->h_addr);
    peer.sin_port = 0; // initialize to invalid port
    soc = socket(AF_INET, SOCK_STREAM, 0);

    // TODO Complete the program below
    fd_set allset;
    FD_ZERO(&allset);
    int max_fd = 0;
    int socs[11];
    int port[11];
    int socs_num = 0;
    for(int i = LOWER; i < UPPER + 1; i++){
        peer.sin_port = i;
        soc = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(soc, (struct sockaddr *)&peer, sizeof(peer)) == -1) {
            printf("Port %d is not listening", i);
            close(soc);
            continue;
        }
        FD_SET(soc, &allset);
        socs[socs_num] = soc;
        port[socs_num] = i;
        socs_num += 1;
        if(soc > max_fd) max_fd = soc
        write(soc, buf, BLOCK_SIZE);
    }
    sleep(100);
    if(select(max_fd + 1, &allset, NULL, NULL, NULL) == -1){
        perror("client: select");
        exit(1);
    }
    for(int i = 0; i < socs_num; i++){
        if(FD_ISSET(socs[i], &allset){
            read(socs[i], buf, BLOCK_SIZE);
            if(strncmp(buf, "HTTP", 4)) == 0){
                printf("Port %d answered with HTTP", port[i]);
            } else {
                printf("Port %d is not a web server", port[i]);
            }
        } else {
            printf("Port %d is not responding", port[i]);
        }
    }
    return 0;
}
