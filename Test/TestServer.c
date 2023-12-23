#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include "../Network.h"
#include "../RDTP/rdt.c"


int main() {
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

    // Receive data from the client
    char buffer[1024];
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    printf("Receiving data...\n");
    int bytesRead = rdt_recv(udp_socket, buffer, sizeof(buffer), 0, 0, &client_addr, &client_addr_len);
    if (bytesRead == SOCKET_ERROR) {
        printf("Failed to receive data.\n");
        closesocket(udp_socket);
        WSACleanup();
        return 1;
    }

    // Process the received data
    printf("Received data from client: %s\n", buffer);

    
    // Send data to the client
    char message[] = "Hello, client! I'm so happy for you. Thank cuthulu this works now.";
    int messageLength = strlen(message);
    if (rdt_send(udp_socket, message, messageLength, 0, 0, (struct sockaddr*)&client_addr, client_addr_len) == SOCKET_ERROR) {
        printf("Failed to send data to the server.\n");
        closesocket(udp_socket);
        WSACleanup();
        return 1;
    }

    // Cleanup
    closesocket(udp_socket);
    WSACleanup();

    return 0;
}
