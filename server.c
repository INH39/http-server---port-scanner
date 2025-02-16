#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h> 


#define MAX_ATTEMPTS 5
#define TIME_WINDOW 10 // in seconds
#define MAX_BLACKLIST 100
#define MAX_CLIENT_TRACKED 250

typedef struct{
    char ip[INET_ADDRSTRLEN];
    int attempts;
    time_t first_attempt;
} ClientRecord;

ClientRecord blacklist[MAX_BLACKLIST]; // store blacklisted clients
ClientRecord attempts[MAX_BLACKLIST]; // store clients' connection attempts
int blacklist_count = 0;
int client_tracked_count = 0;


bool is_blacklisted(const char *ip); // returns true if an ip belongs to a blacklisted client
void add_to_blacklist(const char *ip); // adds an ip to a blacklist
void log_attempt(const char *ip); // monitor frequency of connection attempts made by clients
char *get_client_IP(int client_fd); // returns client's ip address
void handle_request(int client_fd); // handle http request (GET or POST)
int setup_server(int port); // setup server and bind to a specific port
void *client_handler(void *arg); // handle different client requests in a different thread


int main(){
    int server_fd = setup_server(8080);

    while(true){
        int client_fd;
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // accept incoming client connection and store client address in struct
        client_fd = accept(server_fd, (struct sockaddr*) &client_addr, &client_len);

        if(client_fd < 0){
            perror("Accept failed");
            // keep trying until client connects
            continue;
        }
        else{
            printf("Client connected\n");

            // create a new thread to handle requests
            pthread_t thread_id;
            if(pthread_create(&thread_id, NULL, client_handler, (void *) &client_fd) != 0){
                perror("Thread creation failed");

                //if thread creation failed, move on to next connection
                continue;
            }

            // detach thread when done so system cleans up ressources
            pthread_detach(thread_id);

        }
    }

    // close server socket 
    close(server_fd);

    return 0;
}


bool is_blacklisted(const char *ip){
    for(int i = 0; i < blacklist_count; i++){
        // check if a given ip is in blacklist
        if(strcmp(blacklist[i].ip, ip) == 0){
            return true;
        }
    }
    return false;
}


void add_to_blacklist(const char *ip){
    // add to blacklist only if spots are remaining
    if(blacklist_count < MAX_BLACKLIST){
        strncpy(blacklist[blacklist_count].ip, ip, INET_ADDRSTRLEN);
        blacklist_count++;
        printf("Blacklisted IP: %s\n", ip);
    }
}


void log_attempt(const char *ip){
    // get current time
    time_t now = time(NULL);

    for(int i = 0; i < client_tracked_count; i++){
        // first check if client has already made connections attempts
        if(strcmp(attempts[i].ip, ip) == 0){
            // then check if time window is small since first attempt
            if(now - attempts[i].first_attempt < TIME_WINDOW){
                attempts[i].attempts++;
                // check if too many attempts were made from client in a short time
                if(attempts[i].attempts > MAX_ATTEMPTS){
                    add_to_blacklist(ip);
                }
            }
            else{
                // if last attempt exceeds time window then set it as first attempt
                // so we can check if too many attempts are done in another time window
                attempts[i].first_attempt = now;
                attempts[i].attempts = 1; // reset attempts count for that client
            }
        }
    }

    // add new ip to attempts
    strcpy(attempts[client_tracked_count].ip, ip);
    attempts[client_tracked_count].attempts = 1;
    attempts[client_tracked_count].first_attempt = now;
    client_tracked_count++;
}


char *get_client_IP(int client_fd){
    char *client_ip = malloc(INET_ADDRSTRLEN); // standard ipv4 length
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    //get client ip address
    getpeername(client_fd, (struct sockaddr*) &client_addr, &client_len);
    // convert ipv4 address of client from binary to string
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    printf("Client IP: %s\n", client_ip);
    return client_ip;
}


void handle_request(int client_fd){
    char buffer[2048]; // buffer to hold incoming request
    int bytes_received;
    int bytes_sent;

    char *client_ip = get_client_IP(client_fd);

    if(is_blacklisted(client_ip)){
        printf("Connection from a blacklisted IP: %s\n", client_ip);

        // end connection
        free(client_ip);
        close(client_fd);
        return;
    }

    log_attempt(client_ip); // otherwise track connection attempts from this client

    // read the http request from client
    bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0); // sizeof - 1, leave space for null terminator
    if(bytes_received == 0){
        printf("Connection is closed.\n");
    }
    else if(bytes_received < 0){
        perror("Recv failed");
        free(client_ip);
        return;
    }

    // place null terminator after the chars of the request
    buffer[bytes_received] = '\0';

    //log request
    printf("HTTP Request: %s\n", buffer);

    // check type of request by comparing strings
    if(strncmp(buffer, "GET", 3) == 0){
        // handle get requests

        //extract filepath by skipping "Get "
        char *filepath = strtok(buffer + 4, " ");
        if(filepath == NULL){
            filepath = "/"; // default to root directory if no filepath
        }

        // serve file
        int file_fd = open(filepath + 1, O_RDONLY); // skip first "/"
        if(file_fd < 0){
            // error because file was not found
            char response[] = "HTTP/1.1 404 Not found\r\nContent-Type: text/html\r\n\r\n"
                                "<html><body><h1>404 not found</h1></body></html>";
            bytes_sent = send(client_fd, response, sizeof(response), 0);
            if(bytes_sent < 0){
                perror("Send failed");
            }
        }
        else{
            char header[1024];
            //format header
            snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
            bytes_sent = send(client_fd, header, sizeof(header), 0);
            if(bytes_sent < 0){
                perror("Send failed");
                close(file_fd);
                free(client_ip);
                return;
            }

            // send content of file
            while((bytes_received = read(file_fd, buffer, sizeof(buffer))) > 0){
                bytes_sent = send(client_fd, buffer, bytes_received, 0);
                if(bytes_sent < 0){
                    perror("Send failed");
                    break;
                }
            }

            close(file_fd);
        }
    }
    else if(strncmp(buffer, "POST", 4) == 0){
        // handle post requests

        // find content length header in request
        char *content_length_header = strstr(buffer, "Content-Length: ");

        if(content_length_header != NULL){
            // find length of content by skipping content length header
            int content_length = atoi(content_length_header + 16);
            printf("Content-Length: %d\n", content_length);

            // allocate memory for request body
            char request_data[content_length + 1]; // + 1 for null terminator

            // receive data from request and store it
            bytes_received = recv(client_fd, request_data, content_length, 0);
            if(bytes_received == 0){
                printf("Connection is closed.\n");
            }
            else if(bytes_received < 0){
                perror("Receiving data from POST request failed");
                free(client_ip);
                return;
            }

            // place null terminator after the received data
            request_data[bytes_received] = '\0';

            // process data: in this case we just display it
            printf("Data received: %s\n", request_data);

            // send response to client
            char response[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                            "<html><body><h1>POST Request Received</h1></body></html>";
            bytes_sent = send(client_fd, response, sizeof(response), 0);
            if(bytes_sent < 0){
                perror("Send failed");
            }
        }
        else{
            // content-length null means no data
            char response[] = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n"
                            "<html><body><h1>400 Bad Request</h1><p>Missing Content-Length header.</p></body></html>";
            bytes_sent = send(client_fd, response, sizeof(response), 0);
            if(bytes_sent < 0){
                perror("Send failed");
            }
        }
    }
  
    //close client connection
    free(client_ip);
    close(client_fd);
}


int setup_server(int port){
    int server_fd;
    struct sockaddr_in server_addr;

    // create socket
    // communication domain is ipv4 specified by AF_INET
    // communication protocol is TCP (specified with SOCK_STREAM)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // specify server address
    server_addr.sin_family = AF_INET; //specify adress family to ipv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // bind to all interfaces
    // specify port number and convert 16-bit int from host byte order
    // to network byte order , makes sure its in big-endian (store MSB first in memory)
    server_addr.sin_port = htons(port);

    // bind socket to ip address and port
    // here we cast server_addr to the right structure type
    // sockaddr is a generic descriptor for any kind of socket operation instead
    // of sockaddr_in which is specific to ip-based communication
    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0){
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // listen to incoming connections
    int backlog = 10; // max length of queue of pending conn
    if(listen(server_fd, backlog) < 0){
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", port);

    // return server socket file descriptor
    return server_fd;
}


void *client_handler(void *arg){
    // get client fd to process request for connected client socket
    int client_fd = * (int *) arg;

    handle_request(client_fd);

    //exit thread when request in handled
    pthread_exit(NULL);

    return NULL;
}
