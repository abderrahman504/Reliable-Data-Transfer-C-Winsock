#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include "Connection.c"
#include "../Network.h"

// #define MAX_CONNECTIONS 100
// #define TIMEOUT_1 2000 //Timeout if one connection exists
//Array of connections
// ConnectionArgs* connections[MAX_CONNECTIONS];
// int cons_head = 0;

// long long timeout = TIMEOUT_1;
// int no_of_connections = 0;


// void listening(SOCKET listen_socket)
// {
    //Listening on the socket
    // if ( listen( listen_socket, SOMAXCONN ) == SOCKET_ERROR ) {
    //     printf( "Listen failed with error: %ld\n", WSAGetLastError() );
    //     closesocket(listen_socket);
    //     WSACleanup();
    //     return;
    // }

    // while(1)
    // {
    //     //Close any timed out connections and count active connections.
    //     no_of_connections = 0;
    //     for (int i = 0; i < cons_head; i++)
    //     {
    //         if (connections[i]->closed) continue; //skip closed connections
    //         no_of_connections++;
    //         if ((clock() - connections[i]->last_request)/CLOCKS_PER_SEC >= timeout)
    //         {
    //             printf("Connection timed out\n");
    //             shutdown(connections[i]->socket, SD_SEND);
    //             closesocket(connections[i]->socket);
    //             connections[i]->closed = 1;
    //             no_of_connections--;

    //         }
    //     }

    //     //update timeout time
    //     timeout = no_of_connections == 0 ? TIMEOUT_1 : (TIMEOUT_1 / no_of_connections);

    //     int pos_for_new_conn;
    //     if (cons_head != MAX_CONNECTIONS) pos_for_new_conn = cons_head++;
    //     else if (no_of_connections != MAX_CONNECTIONS){
    //         //find the first closed connection 
    //         for(int i = 0; i < cons_head; i++){
    //             if (connections[i]->closed){
    //                 pos_for_new_conn = i;
    //                 break;
    //             }
    //         }
    //     }
        
    //     if (no_of_connections < MAX_CONNECTIONS) //If not at connection capacity
    //     {
    //         printf("Listening...\n");
    //         no_of_connections++;
    //         timeout = TIMEOUT_1 / no_of_connections;
    //         accept_connection(listen_socket, pos_for_new_conn, cons_head == MAX_CONNECTIONS);
    //     }
    // }
    // int debug = 10;
// }

//Accepts a connection and creates a thread to handle it
// void accept_connection(SOCKET listen_socket, int array_pos, int overwrite)
// {
    // struct sockaddr_in clntAddr;
    // int clntAddrLen = sizeof(clntAddr);

    // //Accepting a client
    // SOCKET client_socket = INVALID_SOCKET;
    // client_socket = accept(listen_socket, (struct sockaddr*)&clntAddr,  &clntAddrLen);
    // if (client_socket == INVALID_SOCKET) {
    //     printf("accept failed: %d\n", WSAGetLastError());
    //     return;
    // }


    // // Get the IP Address of the client
    // char clntName[NI_MAXHOST]; // String to contain client address

    // if (getnameinfo((struct sockaddr*)&clntAddr, clntAddrLen, clntName, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0) {
    //     printf("Handling client %s\n", clntName);
    // } else {
    //     perror("Unable to get client address");
    // }

    
    // ConnectionArgs* args = (ConnectionArgs*) malloc(sizeof(ConnectionArgs));
    // args->socket = client_socket;
    // args->closed = 0;
    // args->last_request = clock();
    // if (overwrite) free(connections[array_pos]);
    // connections[array_pos] = args;
    // printf("Creating thread...\n");
    // _beginthread(connection, 0, (void*)args);
// }



typedef struct {
    int seed;
    double lossProbability;
    double errorProbability;
} ServerConfig;


ServerConfig readServerConfig(const char* filePath) {
    ServerConfig config;
    FILE* file = fopen(filePath, "r");
    if (file == NULL) {
        printf("Failed to open file\n");
        exit(1);
    }

    fscanf(file, "Seed: %d", &config.seed);
    fscanf(file, "Loss Probability: %lf", &config.lossProbability);
    fscanf(file, "Error Probability: %lf", &config.errorProbability);

    fclose(file);

    return config;
}


int main(char args[])
{
    printf("Starting server...\n");

    ServerConfig config = readServerConfig("info.txt");
    // printf("Port: %d\n", config.port);
    printf("Seed: %d\n", config.seed);
    printf("Loss Probability: %f\n", config.lossProbability);
    printf("Error Probability: %f\n", config.errorProbability);


    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Failed to initialize Winsock.\n");
        return 1;
    }

    struct addrinfo *server = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;

    int iResult = getaddrinfo(NULL, SERVER_PORT, &hints, &server);
    if (iResult != 0) {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return 1;
    }
    // Create a UDP socket
    SOCKET udp_socket = socket(server->ai_family, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket == INVALID_SOCKET) {
        printf("Failed to create recv socket.\n");
        WSACleanup();
        return 1;
    }

    //bind UDP socket
    if (bind(udp_socket, server->ai_addr, server->ai_addrlen) == SOCKET_ERROR) {
        printf("Failed to bind recv socket.\n");
        closesocket(udp_socket);
        WSACleanup();
        return 1;
    }
    // freeaddrinfo(result);

    printf("Server started\n");
    // listening(udp_socket);
    // srand(config.seed);
    connection(udp_socket, config.lossProbability, config.errorProbability);
    printf("Server closing\n");
    return 0;
}




