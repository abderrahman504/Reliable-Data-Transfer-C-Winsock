#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MSS 500
#define SEGMENT_HEADER_LEN 13
#define ACK_LEN 9

enum {SYN, DATA, FIN};


typedef struct{
    unsigned char type;
    unsigned int len;
    unsigned int seq;
    unsigned int checksum;
    unsigned char data[MSS];
} Segment;


typedef struct{
    unsigned char type;
    unsigned int ack;
    unsigned int checksum;
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

int _get_int_from_stream(unsigned char*);
void _put_int_in_stream(int x, unsigned char*);

//Computes the checksum of a segment from the header and body.
int compute_checksum(Segment* seg){
    int sum = seg->type + seg->len + seg->seq;
    for (int i = 0; i < seg->len; i++) sum += seg->data[i];
    return ~sum;
}

char is_corrupt(Segment* segment){
    int sum = segment->type + segment->len + segment->seq + segment->checksum;
    for(int i=0; i<segment->len; i++) sum += segment->data[i];
    return sum+1 != 0;    
}

int compute_ack_checksum(ACK_Segment* seg){
    return ~seg->ack;
}

char is_ack_corrupt(ACK_Segment* seg){
    int sum = seg->ack + seg->checksum;
    return sum+1 != 0;
}

//Extracts a segment from a char stream
Segment to_segment(unsigned char* stream)
{
    Segment segment = {};
    segment.type = stream[0];
    segment.len = _get_int_from_stream(stream+4);
    segment.seq = _get_int_from_stream(stream+8);
    segment.checksum = _get_int_from_stream(stream+12);
    
    int data_start = 13;
    for (int i=0; i<segment.len; i++) segment.data[i] = stream[data_start+i];
    return segment;
}

char* to_stream(Segment* segment)
{
    char *stream = (char*)malloc(17+(segment->len));
    memset(stream, 0, 17+segment->len);
    stream[0] = segment->type;
    _put_int_in_stream(segment->len, stream+4);
    _put_int_in_stream(segment->seq, stream+8);
    _put_int_in_stream(segment->checksum, stream+12);
    for (int i=0; i<segment->len; i++) stream[13+i] = segment->data[i];
    return stream;
}

ACK_Segment to_ack_segment(unsigned char* stream)
{
    ACK_Segment seg = {};
    seg.type = stream[0];
    seg.ack = _get_int_from_stream(stream+4);
    seg.checksum = _get_int_from_stream(stream+8);
    return seg;
}

char* ack_to_stream(ACK_Segment* segment)
{
    unsigned char* stream = (char*)malloc(9);
    memset(stream, 0, 8);
    stream[0] = segment->type;
    _put_int_in_stream(segment->ack, stream+4);
    _put_int_in_stream(segment->checksum, stream+8);
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
