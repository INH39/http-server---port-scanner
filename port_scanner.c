#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>


#define TARGET_IP "127.0.0.1"
#define START_PORT 8000
#define END_PORT 8100
#define TIMEOUT_SEC 1 
#define BUFFER_SIZE 1024


int scan_port(const char *ip, int port); // scan open ports of server that's targeted


int scan_port(const char *ip, int port){
    // create socket for tcp connection
    struct sockaddr_in target_serv;
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if(sock < 0){
        perror("Socket creation failed.\n");
        return -1;
    }

    // specify timeout (to avoid long wait times)
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;

    // set timeout on socket receive operation
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // specify target serv info
    target_serv.sin_family = AF_INET;
    target_serv.sin_port = htons(port);
    inet_pton(AF_INET, ip, &target_serv.sin_addr);

    // attempt to connect socket to target server
    int result = connect(sock, (struct sockaddr*) &target_serv, sizeof(target_serv));
    if(result == 0){
        printf("Port %d is open.\n", port);

        // send http request to server
        const char *request = "GET /test_get.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
        send(sock, request, sizeof(request), 0);
        
        // receive response from server
        char buffer[BUFFER_SIZE];
        int bytes_received = recv(sock, buffer, sizeof(buffer), 0);

        if(bytes_received < 0){
            perror("Receive failed.");
        }
        else if(bytes_received > 0){
            buffer[bytes_received] = '\0';

            // print response
            printf("Response: %s\n", buffer);
        }
        else{
            printf("No response received.\n");
        }
    }

    // close socket
    close(sock);

    return result;
}


int main(){
    printf("Scanning from port %d to port %d . . .\n", START_PORT, END_PORT);

    // scan through ports and check if they are open
    for(int port = START_PORT; port <= END_PORT; port++){
        int result = scan_port(TARGET_IP, port);

        if(result < 0){
            printf("Port %d is closed.\n", port);
        }
    }
    printf("Scan completed.\n");

    return 0;
}