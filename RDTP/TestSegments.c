#include <stdio.h>
#include "Segment.c"


int main(int argc, char const *argv[])
{

    int x = 0x12345678;
    char ch[10] = {};
    // _put_int_in_stream(x, ch+4);
    ACK_Segment ack;
    ack.type = 0;
    ack.ack = 12345;
    ack.checksum = compute_ack_checksum(&ack);

    char *seg_stream = ack_to_stream(&ack);
    ACK_Segment ack2 = to_ack_segment(seg_stream);
    // Segment b = to_segment(seg_stream);
    return 0;
}
