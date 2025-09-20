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


bool is_blacklisted(const char *ip); 
void add_to_blacklist(const char *ip); 
void log_attempt(const char *ip); 
char *get_client_IP(int client_fd); 
void handle_request(int client_fd); 
int setup_server(int port); 
void *client_handler(void *arg); 


int main(){
    int server_fd = setup_server(8080);

    while(true){
        int client_fd;
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        client_fd = accept(server_fd, (struct sockaddr*) &client_addr, &client_len);

        if(client_fd < 0){
            perror("Accept failed");
            continue;
        }
        else{
            printf("Client connected\n");

            pthread_t thread_id;
            if(pthread_create(&thread_id, NULL, client_handler, (void*)(intptr_t) client_fd) != 0){
                perror("Thread creation failed");

                continue;
            }

            pthread_detach(thread_id);
        }
    }

    // close server socket 
    close(server_fd);

    return 0;
}


bool is_blacklisted(const char *ip){
    for(int i = 0; i < blacklist_count; i++){

        if(strcmp(blacklist[i].ip, ip) == 0){
            return true;
        }
    }
    return false;
}


void add_to_blacklist(const char *ip){

    if(blacklist_count < MAX_BLACKLIST){
        strncpy(blacklist[blacklist_count].ip, ip, INET_ADDRSTRLEN);
        blacklist_count++;
        printf("Blacklisted IP: %s\n", ip);
    }
}


void log_attempt(const char *ip){
    time_t now = time(NULL);

    for(int i = 0; i < client_tracked_count; i++){
        if(strcmp(attempts[i].ip, ip) == 0){

            if(now - attempts[i].first_attempt < TIME_WINDOW){
                attempts[i].attempts++;

                if(attempts[i].attempts > MAX_ATTEMPTS){
                    add_to_blacklist(ip);
                }
            }
            else{
                attempts[i].first_attempt = now;
                attempts[i].attempts = 1; 
            }
        }
    }

    strcpy(attempts[client_tracked_count].ip, ip);
    attempts[client_tracked_count].attempts = 1;
    attempts[client_tracked_count].first_attempt = now;
    client_tracked_count++;
}


char *get_client_IP(int client_fd){
    char *client_ip = malloc(INET_ADDRSTRLEN); // standard ipv4 length
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    getpeername(client_fd, (struct sockaddr*) &client_addr, &client_len);
    // convert ipv4 address of client from binary to string
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    printf("Client IP: %s\n", client_ip);
    return client_ip;
}


void handle_request(int client_fd){
    char buffer[2048]; // buffer forincoming request
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

    // read request from client
    bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if(bytes_received == 0){
        printf("Connection is closed.\n");
    }
    else if(bytes_received < 0){
        perror("Recv failed");
        free(client_ip);
        return;
    }

    buffer[bytes_received] = '\0';

    //log request
    printf("HTTP Request: %s\n", buffer);

    if(strncmp(buffer, "GET", 3) == 0){

        char *filepath = strtok(buffer + 4, " ");
        if(filepath == NULL){
            filepath = "/"; // default to dir
        }

        // serve file
        int file_fd = open(filepath + 1, O_RDONLY);
        if(file_fd < 0){
            // file not found
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
        char *content_length_header = strstr(buffer, "Content-Length: ");

        if(content_length_header != NULL){
            int content_length = atoi(content_length_header + 16);
            printf("Content-Length: %d\n", content_length);

            char *body_start = strstr(buffer, "\r\n\r\n");
            
            if(body_start != NULL){
                body_start += 4; // skip past \r\n\r\n
                int body_received = bytes_received - (body_start - buffer);
                
                if(body_received >= content_length){
                    // body already received in first recv()
                    body_start[content_length] = '\0';
                    printf("Data received: %s\n", body_start);
                } else {
                    // need to receive more body data
                    char request_data[content_length + 1];
                    strncpy(request_data, body_start, body_received);
                    
                    int remaining = content_length - body_received;
                    int additional_bytes = recv(client_fd, request_data + body_received, remaining, 0);
                    
                    if(additional_bytes > 0){
                        request_data[body_received + additional_bytes] = '\0';
                        printf("Data received: %s\n", request_data);
                    }
                }
            } 
            else {
                printf("No body separator found\n");
            }

            // send response
            char response[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                            "<html><body><h1>POST Request Received</h1></body></html>";
            bytes_sent = send(client_fd, response, sizeof(response), 0);
            if(bytes_sent < 0){
                perror("Send failed");
            }
        }
        else{
            // no data in request
            char response[] = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n"
                            "<html><body><h1>400 Bad Request</h1><p>Missing Content-Length header.</p></body></html>";
            bytes_sent = send(client_fd, response, sizeof(response), 0);
            if(bytes_sent < 0){
                perror("Send failed");
            }
        }
    }
  
    //close client conn
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

    server_addr.sin_family = AF_INET; // ipv4
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(port); // host byte order to network byte order

    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0){
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    int backlog = 10; // pending connections queue max length
    if(listen(server_fd, backlog) < 0){
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", port);

    return server_fd;
}


void *client_handler(void *arg){
    int client_fd = (int)(intptr_t) arg;
    handle_request(client_fd);
    //exit thread when request is handled
    pthread_exit(NULL);

    return NULL;
}
