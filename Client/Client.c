#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../Network.h"

#define DEFAULT_COMMANDS "commands.txt"
#define BUFFER 10000

void start(FILE*, SOCKET);
int run_command(char*, char*, SOCKET);
int connect_to_server(char*, char*, SOCKET*);
// int handle_get(SOCKET, char*, char*, char*);
// int handle_post(SOCKET, char*, char*, char*);


/*
This function runs the commands in file one by one.
It creates a socket with the specified server name and port and sends the specified request.
After processing the response it shuts the socket down and repeats until end of file.
*/

typedef struct {
    char ipAddress[16];
    int port;
    char filename[256];
} clientRead;

clientRead clientReadInfo(const char* filePath) {
    clientRead info;
    FILE* file = fopen(filePath, "r");
    if (file == NULL) {
        printf("Failed to open file\n");
        exit(1);
    }

    fscanf(file, "%s", info.ipAddress);
    fscanf(file, "%d", &info.port);
    fscanf(file, "%s", info.filename);

    fclose(file);

    return info;
}

void start(FILE* file, SOCKET conn)
{

    clientRead info = clientReadInfo("info.txt");
    printf("IP Address: %s\n", info.ipAddress);
    printf("Port: %d\n", info.port);
    printf("Filename: %s\n", info.filename);
    // char recv_buf[BUFFER];
    // char line[100];
    // // char DEFAULT_PORT[] = "80";
    // int error_count = 0;
    // printf("Reading from commands file...\n");
    // char* check = fgets(line, 100, file);
    // while (check != NULL) //While more commands exist.
    // {
    //     printf("Next command...\n");
    //     //Parsing line from commands file
    //     char* token = strtok(line, " ");
    //     char method[5];
    //     if (strcmp(token, "client_get") == 0) strcpy(method, "GET");
    //     else strcpy(method, "POST");
    //     char *file_path = strtok(NULL, " ");
    //     char *servername = strtok(NULL, " ");
    //     char *port = strtok(NULL, "\n");
    //     // if (port == NULL) port = DEFAULT_PORT;
       
    //     //Performing command
    //     error_count += run_command(method, file_path, conn);
    //     //Fetch next command
    //     check = fgets(line, 100, file);
    // }
    // printf("Command file finished with %d errors.\n", error_count);
    // shutdown(conn, SD_SEND);
    // closesocket(conn);
}


/*
Runs the given command.
Creates a socket to the specified server and sends the request.
Returns 1 if there was an error with doing the command, and 0 otherwise.
*/
int run_command(char* method, char* file_path, SOCKET conn)
{
    if (strcmp(method, "GET") == 0) //GET request
    {
        handle_get(conn, file_path);
    }
    else //POST request
    {
        handle_post(conn, file_path);
    }
    return 0;
}


int handle_get(SOCKET conn, char* path)
{
    //Construct request
    char request[128] = "";
    strcat(request, "GET ");
    strcat(request, "/");
    strcat(request, path);
    strcat(request, " HTTP/1.1\r\n\r\n");
    //Send request
    printf("Sending GET to server...\n");
    int res = send(conn, request, 128, 0);
    if (res == SOCKET_ERROR){
        printf("send failed with error: %d\n", WSAGetLastError());
        // closesocket(conn);
        return 1;
    }

    //Receive response
    char response[BUFFER];
    printf("Recieving response...\n");

    int bytesReceived;

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    // bytesReceived = recv(conn, response, BUFFER, 0);
    // printf("%d\n",bytesReceived);

    do {
        bytesReceived = recv(conn, response, BUFFER, 0);
        printf("--------------------------------------------------------------%d\n",bytesReceived);

        if (bytesReceived < 0) {
            perror("Error receiving data");
            return 1;
        }

        if (bytesReceived == 0){
            printf("Received nothing\n");
            break;
        } else if (bytesReceived < 0){
            printf("rcv failed with error %d\n",  WSAGetLastError());
            return 1;
        }

        strtok(response, " ");
        char* status = strtok(NULL, " ");
        printf("status : %s\n",status);
        printf("bytesReceived : %d\n",bytesReceived);
        if (strcmp(status, "200") == 0) //OK
        {
            printf("\n"); //Print newline after requested file body
        //     //read blank line
            strtok(NULL, "\n");
            char *token = strtok(NULL, "\n");
            char* body = token + (strlen(token)+1);
            printf("body : %s\n",body);

            printf("--------------------------------------------------------------%d\n",bytesReceived);

            fwrite(body, 1, bytesReceived-19, file);
        }

        if(bytesReceived < BUFFER) break;

        

        // Write the received data to the file
        // fwrite(response, 1, bytesReceived, file);

    } while (bytesReceived-19 > 0);

    fclose(file);
    printf("File stored\n");

        
    

    // if (bytesReceived < 0) {
    //     perror("Error receiving data");
    // }

    // if (bytesReceived == 0){
    //     printf("Received nothing\n");
    //     return 1;
    // } else if (bytesReceived < 0){
    //     printf("rcv failed with error %d\n",  WSAGetLastError());
    //     return 1;
    // }
    //print response
    // printf(response);
    //read status
    // strtok(response, " ");
    // char* status = strtok(NULL, " ");
    // printf("status : %s\n",status);
    // if (strcmp(status, "200") == 0) //OK
    // {
    //     printf("\n"); //Print newline after requested file body
    // //     //read blank line
    //     strtok(NULL, "\n");
    //     char *token = strtok(NULL, "\n");
    //     char* body = token + (strlen(token)+1);
    //     printf("body : %s\n",body);
    // //     //read file and store it

    //     FILE *file = fopen(path, "wb");
    //     if (file == NULL) {
    //         perror("Error opening file");
    //         return 1;
    //     }
    //     // Write the received data to the file
    //     fwrite(body, 1, bytesReceived-19, file);
    //     fclose(file);
    //     printf("File stored\n");

    // }
    return 0;
}

int handle_post(SOCKET conn, char* path)
{
    printf("Hello from posttt\n");
    char request[BUFFER] = "";
    strcat(request, "POST ");
    strcat(request, "/");
    strcat(request, path);
    strcat(request, " HTTP/1.1\r\n\r\n");
    //Read file
    FILE* f = fopen(path, "rb");
    if (f==NULL){
        printf("Failed to open file to be posted\n");
        return 1;
    }
    int requestlen = strlen(request);
    requestlen += fread(request+requestlen, 1, BUFFER-requestlen, f);
    //Send request.
    printf("Sending POST to server...\n");
    int res = send(conn, request, requestlen, 0);
    if (res == SOCKET_ERROR){
        printf("send failed with error: %d\n", WSAGetLastError());
        // closesocket(conn);
        return 1;
    }

    char chunk[BUFFER];
    size_t bytesRead;
    do {
        bytesRead = fread(chunk, 1, sizeof(chunk), f);
        if (bytesRead > 0) {
            res = send(conn, chunk, bytesRead, 0);
            if (res == SOCKET_ERROR){
                printf("send failed with error: %d\n", WSAGetLastError());
                return 1;
            }
        }
    } while (bytesRead > 0);

    fclose(f);

    //Receive response
    char response[BUFFER];
    printf("Recieving response...\n");
    res  = recv(conn, response, BUFFER, 0);
    if (res == 0){
        printf("Received nothing\n");
        return 1;
    } else if (res < 0){
        printf("rcv failed with error %d\n",  WSAGetLastError());
        return 1;
    }
    //print response
    printf(response);
    return 0;
}


/*
Creates and connects a socket to the specified server and port.
conn is an output pointer that contains the created socket.
Returns 0 if connected succesfully, and 1 otherwise.
*/
int connect_to_server(char* servername, char* port, SOCKET* conn_ptr)
{
    int iResult;
    //Prep socket creation
    struct addrinfo *result = NULL, hints;
    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo(servername, port, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed: %d\n", iResult);
        return 1;
    }
    //Create socket
    *conn_ptr = INVALID_SOCKET;
    *conn_ptr = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (*conn_ptr == INVALID_SOCKET) {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        return 1;
    }

    //Connecting to server
    iResult = connect(*conn_ptr, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);
    if (iResult == SOCKET_ERROR){
        // closesocket(*conn_ptr);
        *conn_ptr = INVALID_SOCKET;
    }
    if (*conn_ptr == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        return 1;
    }
    return 0;
}

/*
Running from command line:
./Client.exe
./Client.exe [server_ip] [server_port]
./Client.exe [server_ip] [server_port] [commands_file]
*/
int main(int argc, char *argv[])
{
    char server_name[16], server_port[8], command_file[32];
    //Checking arguments for server name, port, commands file.
    if (argc == 1){
        printf("Client running with no arguments. Using localhost, 80 and commands.txt...\n");
        strcpy(server_name, SERVER_NAME);
        strcpy(server_port, SERVER_PORT);
        strcpy(command_file, DEFAULT_COMMANDS);
    }
    else if (argc == 3){
        printf("Client running with 2 arguments-server name and port-with commands.txt as default commands file...\n");
        strcpy(server_name, argv[1]);
        strcpy(server_port, argv[2]);
        strcpy(command_file, DEFAULT_COMMANDS);
    }
    else if (argc == 4){
        printf("Client running with 3 arguments-server name, port, and commands file...\n");
        strcpy(server_name, argv[1]);
        strcpy(server_port, argv[2]);
        strcpy(command_file, argv[3]);
    }
    else{
        printf("Invalid number of arguments (%d). Shutting client down\n", argc);
        return 1;
    }
    //Trying to open commands file
    FILE* file = fopen(command_file, "r");
    if (file == NULL){
        printf("Couldn't open commands file %s. Make sure this file exists\n. Closing client.\n", command_file);
        return 1;
    }
    // Initialize Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }
    SOCKET conn = INVALID_SOCKET;
    if (connect_to_server(server_name, server_port, &conn) == 0){
        printf("Client connected to server\n");
        start(file, conn);
    }
    WSACleanup();
    printf("Client shutting down.\n");
    return 0;
}
