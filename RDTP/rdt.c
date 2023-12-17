#include <winsock2.h>
#include "Segment.c"
#include <math.h>


//Reliable function for sending data through a socket.
int rdt_send(SOCKET socket, char* buffer, int len, int flags)
{
    //Break up buffer into segments
    Segment segments[(int)ceil((double)len/MSS)];
    for(int buf_head=0; buf_head<len; buf_head+=MSS)
    {
        Segment segment;
        segment.type = DATA;
        segment.len = len-buf_head > MSS ? MSS : len-buf_head;
        segment.seq = buf_head;
        for(int i=buf_head; i<buf_head+segment.len; i++)
        {
            segment.data[i-buf_head] = buffer[i];
        }
        segment.checksum = compute_checksum(&segment);
        segments[buf_head/MSS] = segment;
    }
    
    return 0;
}

//Reliable function for receiving data from a socket.
int rdt_recv(SOCKET socket, char* buffer, int len, int flags)
{

    return 0;
}