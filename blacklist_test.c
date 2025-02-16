#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>



// simulated ip addresses to test server's blacklist
const char *fake_ips[] = {
    "192.168.1.100",
    "192.168.1.101",
    "192.168.1.102",
    "192.168.1.103",
    "192.168.1.104",
    "77.225.74.152",
    "164.89.49.98",
    "173.27.88.163",
    "58.13.114.102",
    "154.158.250.27",
    "107.208.199.150",
    "119.153.85.13",
    "129.70.145.58",
    "88.214.173.148",
    "140.93.238.245",
    "110.47.128.198",
    "185.100.155.164",
    "79.18.162.224",
    "46.249.167.177",
    "87.167.18.108",
    "6.76.112.254",
    "145.127.131.129",
    "160.204.72.111",
    "124.62.164.155",
    "29.216.53.130",
    "40.44.92.147",
    "5.123.74.97",
    "210.202.251.255",
    "13.254.91.180",
    "234.119.202.106",
    "96.201.215.14",
    "195.189.90.146",
    "14.68.133.90",
    "24.23.127.171",
    "35.135.118.107",
    "218.1.59.195",
    "114.97.233.23",
    "135.106.178.94",
    "226.109.160.46",
    "202.241.19.95",
    "9.224.180.66",
    "80.153.85.39",
    "55.126.20.220",
    "215.69.251.97",
    "12.138.213.217",
    "40.157.77.230",
    "90.236.40.253",
    "115.45.184.2",
    "186.163.181.38",
    "72.24.249.3",
    "108.173.145.60",
    "150.176.227.100",
    "178.6.242.144",
    "199.46.25.199",
    "151.68.201.196"
};

#define FAKE_IP_COUNT (sizeof(fake_ips) / sizeof(fake_ips[0]))
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080


// test server's blacklist using IP rotation and a delay to avoid detection
void attack_server();


void attack_server(){
    struct sockaddr_in server_addr;
    int sock;

    printf("Starting IP rotation attack . . .\n");

    for(int i = 0; i < FAKE_IP_COUNT; i++){
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if(sock < 0){
            perror("Socket creation failed.\n");

            // continue connections with other ip addresses
            continue;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

        // simulate changing ip address (IRL it requires raw sockets)
        printf("Attempt #%d - Simulating connection from %s\n", i + 1, fake_ips[i]);

        if(connect(sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0){
            perror("Connection failed.\n");
        }
        else{
            printf("Connection was a success.\n");
            close(sock);
        }
        sleep(1); // delay
    }
}


int main(){
    attack_server();

    return 0;
}