#include <stdio.h>
#include "Segment.c"


int main(int argc, char const *argv[])
{

    int x = 0x12345678;
    char ch[10] = {};
    // _put_int_in_stream(x, ch+4);
    Segment segment;
    segment.type = 100;
    segment.len = 23;
    segment.seq = 0xe48a7bef;
    segment.ack = 0x00456100;
    segment.checkSum = 0x00000000;
    char *seg_stream;
    seg_stream = to_char_stream(segment);
    Segment b = to_segment(seg_stream);
    return 0;
}
