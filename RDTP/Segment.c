#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MSS 500

enum {SYN, DATA, FIN};

typedef struct{
    int type;
    int len;
    int seq;
    int checksum;
    char data[MSS];
} Segment;


typedef struct{
    int ack;
    int checksum;
} ACK_Segment;

/*
datagram structure and length
0 byte: type
1-4 bytes: len
5-8 bytes: seq
9-12 bytes: checksum
13-end bytes: data

header size = 13 bytes
*/

//Computes the checksum of a segment from the header and body.
int computeCheckSum(Segment* seg)
{
    int sum = seg->type + seg->len + seg->seq;
    for (int i = 0; i < seg->len; i++) sum += seg->data[i];
    return ~sum;
}

char isCorrupt(Segment* segment)
{
    int sum = segment->type + segment->len + segment->seq + segment->checksum;
    for(int i=0; i<segment->len; i++) sum += segment->data[i];
    return sum != 0;    
}

//Extracts a segment from a char stream
Segment to_segment(unsigned char* stream)
{
    Segment segment = {};
    segment.type = stream[0];
    segment.len = _get_int_from_stream(stream+4);
    segment.seq = _get_int_from_stream(stream+8);
    segment.checksum = _get_int_from_stream(stream+12);
    // segment.checksum = _get_int_from_stream(stream+16);
    
    int data_start = 13;
    for (int i=0; i<segment.len; i++) segment.data[i] = stream[data_start+i];
    return segment;
}

char* to_char_stream(Segment segment)
{
    char *stream = (char*)malloc(17+segment.len);
    memset(stream, 0, 17+segment.len);
    stream[0] = segment.type;
    _put_int_in_stream(segment.len, stream+4);
    _put_int_in_stream(segment.seq, stream+8);
    _put_int_in_stream(segment.checksum, stream+12);
    // _put_int_in_stream(segment.checksum, stream+16);
    for (int i=0; i<segment.len; i++) stream[13+i] = segment.data[i];
    return stream;
}

void _put_int_in_stream(int x, unsigned char* low_order_byte)
{
    *low_order_byte = x & 0xff;
    *(low_order_byte-1)= (x & 0xff00) >> 8;
    *(low_order_byte-2)= (x & 0xff0000) >> 16;
    *(low_order_byte-3)= (x & 0xff000000) >> 24;
}

int _get_int_from_stream(unsigned char* low_order_byte)
{
    int x = (*low_order_byte) | *(low_order_byte-1)<<8 | *(low_order_byte-2)<<16 | *(low_order_byte-3)<<24;
    return x;
}
