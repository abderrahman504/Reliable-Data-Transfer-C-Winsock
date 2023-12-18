#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
// #include <process.h>
#include "Connection.c"
#include "../Network.h"
#include "../RDTP/rdt.c"



int main(char args[])
{
    printf("Starting server...\n");
    // Initialize Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
    printf("WSAStartup failed: %d\n", iResult);
    return 1;
    }

    //Preparing to create a socket
    struct addrinfo *server = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, SERVER_PORT, &hints, &server);
    if (iResult != 0) {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    //Creating a listening socket
    SOCKET udp_socket = INVALID_SOCKET;
    udp_socket = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (udp_socket == INVALID_SOCKET) {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(server);
        WSACleanup();
        return 1;
    }

    //Binding the socket
    iResult = bind(udp_socket, server->ai_addr, (int)server->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(server);
        closesocket(udp_socket);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(server);

    printf("Server started\n");

    start(udp_socket);

    printf("Server closing\n");
    return 0;
}


void start(SOCKET udp_socket)
{
    ConnectionArgs args = {};
    args.closed = 0;
    args.socket = udp_socket;
    connection((void *) &args);
}