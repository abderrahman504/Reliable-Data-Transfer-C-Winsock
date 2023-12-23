#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../Network.h"
#include "../RDTP/rdt.c"


int main() {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Failed to initialize Winsock.\n");
        return 1;
    }

    // Create a exchange sockets
    SOCKET udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket == INVALID_SOCKET) {
        printf("Failed to create send socket.\n");
        WSACleanup();
        return 1;
    }


    // Set up server address
    struct addrinfo *server = NULL, hints;
    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    int iResult = getaddrinfo("localhost", SERVER_PORT, &hints, &server);
    if (iResult != 0) {
        printf("getaddrinfo failed: %d\n", iResult);
        return 1;
    }


    // Send data to the server
    char message[] = "Hello, server! I am the client. Thank god this works now";
    int messageLength = strlen(message);
    if (rdt_send(udp_socket, message, messageLength, 0, 0, server->ai_addr, server->ai_addrlen) == SOCKET_ERROR) {
        printf("Failed to send data to the server.\n");
        closesocket(udp_socket);
        WSACleanup();
        return 1;
    }

    //Receive data from server
    char buffer[1024];
    printf("Receiving data...\n");
    int bytesRead = rdt_recv(udp_socket, buffer, sizeof(buffer), 0, 0, (struct sockaddr_in*)server->ai_addr, &(server->ai_addrlen));
    if (bytesRead == SOCKET_ERROR) {
        printf("Failed to receive data.\n");
        closesocket(udp_socket);
        WSACleanup();
        return 1;
    }

    printf("Received from server: %s\n", buffer);
    // Cleanup
    closesocket(udp_socket);
    WSACleanup();

    return 0;
}
