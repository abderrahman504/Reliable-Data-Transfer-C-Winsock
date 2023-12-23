#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../RDTP/rdt.c"

#define BUFFER_LENGTH 10000

#define NOT_FOUND "HTTP/1.1 404 Not Found\r\n"
#define OK "HTTP/1.1 200 OK\r\n"
#define BLANK_LINE "\r\n"

typedef struct {
    SOCKET socket;
    clock_t last_request;
    char closed;
}ConnectionArgs;



void print_optns();
void parse_rqln(char*, char*, char*);
void parse_request(char*, char*, char*);
int handle_get(SOCKET, char*, float, float, struct sockaddr*, int);
int handle_post(SOCKET,char*, int);
int handle_request(char*, SOCKET, int, float, float, struct sockaddr*, int);


void connection(SOCKET s, float plp, float pep)
{
    char recv_buf[BUFFER_LENGTH];

    //Waiting for a request from the client
    struct sockaddr client;
    int client_len;
    memset(recv_buf, 0, sizeof(recv_buf));
    int received = rdt_recv(s, recv_buf, BUFFER_LENGTH, plp, pep, (struct sockaddr_in*)&client, &client_len);
    if (received > 0)
    {
        printf("Bytes received: %d\n", received);
        printf("Received: %s\n", recv_buf);
        
        int sendRes = handle_request(recv_buf, s, received, plp, pep, &client, client_len);
        if (sendRes == SOCKET_ERROR)
        {
            printf("response failed with error: %d\n", WSAGetLastError());
            closesocket(s);
            return;
        }
    }
    else if (received == 0)
    {
        //Means the client closed their socket
        closesocket(s);
        printf("Client connection closed\n");
        return;
    }
    else
    {
        printf("recv failed with error: %d\n", WSAGetLastError());
        closesocket(s);
        return;
    }
    printf("Closing connection...\n");
    int res = closesocket(s);
    if (res == SOCKET_ERROR)
    {
        printf("closesocket failed with error: %d\n", WSAGetLastError());
        return;
    }
}


//---------------------------REQUEST HANDLING----------------------------//
/*
Handles request in buffer
*/
int handle_request(char buffer[], SOCKET socket, int received, float plp, float pep, struct sockaddr* client, int client_len)
{
    char method[10];
    char path[100];
    sscanf(buffer, "%s /%s",method,path);

    if(strcmp(method,(char*)"GET")==0){
        
        printf("GET\n");
        printf("%s\n",path);
        return handle_get(socket, path, plp, pep, client, client_len);
    }
    if(strcmp(method,(char*)"POST")==0){
        // post_request();
        printf("POST\n");
        return handle_post(socket,buffer, received);
    }
}

int handle_get(SOCKET socket, char* path, float plp, float pep, struct sockaddr* client, int client_len)
{
    printf("Opening %s...\n", path);
    FILE *file = fopen(path, "rb");
    if (file == NULL) //File not found
    {
        printf("Failed to open file\n");
        char response[100] = "";
        strcat(response, NOT_FOUND);
        strcat(response, BLANK_LINE);
        printf("%s",response);
        return rdt_send(socket, response, 100, plp, pep, client, client_len);
    }
    else //File found
    {
        
        char response[BUFFER_LENGTH] = "";
        strcat(response,OK);
        strcat(response,BLANK_LINE);

        // size_t bytesRead = fread(response+19, 1, sizeof(response)-19, file);
        // printf("bytesRead = %d\n",bytesRead);
        // printf("%s",response);
        // int result = send(socket, response, bytesRead+19 , 0);
        size_t bytesRead;
        int result;
        
        // if(result==SOCKET_ERROR){
        //     printf("error while sending\n");
        // }

        do {
            bytesRead = fread(response+19, 1, sizeof(response)-19, file);
            if(bytesRead == 0)
                break;
            printf("bytesRead = %d\n", bytesRead);
            printf("%s", response);

            result = rdt_send(socket, response, bytesRead+19 , plp, pep, client, client_len);
            if(result == SOCKET_ERROR){
                printf("error while sending\n");
                break;
            }
        } while (bytesRead > 0);

        // Clean up: close the file and free the allocated memory
        fclose(file);
        // free(file_content);

        printf("data sentt\n");

        return result;

    }
}

int handle_post(SOCKET socket,char* buffer, int received)
{
    //implement later
    printf("handling POST ...");
    char path[100],method[10];
    printf("buffer: %s",buffer);
    sscanf(buffer, "%s /%s",method,path);
    printf("the path is: %s",path);


    // Find the start of the request body
    char *bodyStart = strstr(buffer, "\r\n\r\n");
    int result;
    if (bodyStart != NULL) {
        bodyStart += 4;  // Move past the empty line
        printf("buffer: %s",bodyStart);
        FILE* file = fopen(path, "wb");
        if (file == NULL) {
            result = send(socket,NOT_FOUND,sizeof(NOT_FOUND),0);
            perror("Error opening file");
            return result;
        }
        
        fwrite(bodyStart, 1, received - (bodyStart - buffer), file);

        // Receive the rest of the body in chunks
        char chunk[BUFFER_LENGTH];
        int bytesReceived;
        do {
            bytesReceived = recv(socket, chunk, sizeof(chunk), 0);
            if (bytesReceived > 0) {
                fwrite(chunk, 1, bytesReceived, file);
            } else if (bytesReceived < 0) {
                perror("Error receiving data");
                return 1;
            }

            if(bytesReceived < BUFFER_LENGTH)
                break;
        } while (bytesReceived > 0);

        char response[100];
        strcat(response, OK);
        strcat(response, BLANK_LINE);

        result = send(socket,response,100,0);
        fclose(file);
        return result;

    }
    else{

        char response[100];
        strcat(response, NOT_FOUND);
        strcat(response, BLANK_LINE);
        
        result = send(socket,response,100,0);
        perror("no message content");

        return result;
    }
}

//---------------------------END OF REQUEST HANDLING----------------------------//