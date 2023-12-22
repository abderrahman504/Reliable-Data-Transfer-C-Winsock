#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <stdlib.h>
#include "Segment.c"
#include <math.h>


#define TIMEOUT_MICROSEC 100000;

enum {UNSENT, SENT, RESENT, ACKED};
enum {SLOW_START, CONGESTION_AVOIDANCE, FAST_RECOVERY};

typedef struct {
    char state;
    clock_t last_sent;
} Marker;


//Reliable function for sending data through a socket using a TCP-like policy.
//Assumes selective acknowledgement
int rdt_send(SOCKET socket, char* buffer, int len, struct sockaddr *dest, int dest_len, float plp, float pep)
{
    //Break up buffer into segments
    int no_of_segments = (int)ceil((double)len/MSS);
    Segment segments[no_of_segments];
    for(int buf_head=0; buf_head<len; buf_head+=MSS)
    {
        Segment segment;
        segment.type = DATA;
        segment.len = len-buf_head > MSS ? MSS : len-buf_head;
        segment.seq = buf_head;
        for(int i=buf_head; i<buf_head+segment.len; i++)
            segment.data[i-buf_head] = buffer[i];
        segment.checksum = compute_checksum(&segment);
        segments[buf_head/MSS] = segment;
    }

    //Set socket to block recv calls for sending SYN
    struct timeval tv = {};
    tv.tv_usec = TIMEOUT_MICROSEC;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv)) {
        perror("setsockopt");
        return -1;
    }

    //Send SYN segment
    Segment init_seg = {};
    init_seg.len = 0;
    init_seg.type = SYN;
    init_seg.seq = segments[no_of_segments-1].seq;
    init_seg.checksum = compute_checksum(&init_seg);
    int send_res;
    char recv_stream[ACK_LEN];
    int recv_res;
    while(1)
    {
        printf("Sending SYN segment...\n");
        send_res = _transmit_segment(&init_seg, NULL, plp, pep, socket, dest, dest_len);
        if (send_res == -1)
        {
            perror("sending SYN");
            return -1;
        }
        else if (send_res != 0)
        {
            printf("Didn't send all of SYN segment. No action is taken\n");
        }
        recv_res = recvfrom(socket, recv_stream, ACK_LEN, 0, dest, dest_len);
        if (recv_res < 0)
        {
            if(errno == EWOULDBLOCK || errno == EAGAIN)
            {
                printf("Timeout. Resending SYN segment...\n");
                continue;
            }
            else{
                perror("recv SYN ACK");
                return -1;
            }
        }
        else 
        {
            ACK_Segment ack = to_ack_segment(recv_stream);
            if (ack.type != SYN || is_ack_corrupt(&ack))
            {
                printf("Corrupted ACK. Resending SYN segment...\n");
                continue;
            }
            else break;
        }
    }

    // Change socket to non-blocking mode
    u_long mode = 1;  // 1 to enable non-blocking mode, 0 to disable
    if (ioctlsocket(socket, FIONBIO, &mode) != NO_ERROR) {
        printf("ioctlsocket failed with error: %u\n", WSAGetLastError());
        return -1;
    }
    //Timeout calculation variables
    int estimatedRTT = 0;
    int devRTT = 0;
    int timeout_usec = 1000;
    //Congestion control variables
    int cwnd = MSS;
    int ssthresh = 64000;
    int cur_seg = 0;
    int dup_ack_count = 0;
    int state = SLOW_START;
    Marker markers[no_of_segments];
    for(int i=0; i<no_of_segments; i++)
        markers[i].state = UNSENT;

    //Send segments
    _transmit_cwnd(cwnd, segments, cur_seg, no_of_segments, markers, plp, pep, socket, dest, dest_len);
    while(cur_seg < no_of_segments)
    {
        recv_res = recvfrom(socket, recv_stream, ACK_LEN, 0, dest, dest_len);
        if (recv_res < 0)
        {
            if(errno == EWOULDBLOCK) //received nothing
            {
                clock_t now = clock();
                int time_msec = ((double)now-markers[cur_seg].last_sent) / CLOCKS_PER_SEC * 1000;
                if (time_msec >= timeout_usec) //timer expired
                {
                    printf("Timeout.\n");
                    ssthresh = cwnd / 2;
                    cwnd = MSS;
                    dup_ack_count = 0;
                    time_msec *= 2;
                    _transmit_segment(segments+cur_seg, markers+cur_seg, plp, pep, socket, dest, dest_len);
                    state = SLOW_START;
                }
                continue;
            }
            else{
                perror("recv ACK");
                return -1;
            }
        }
        else //received something
        {
            ACK_Segment ack_seg = to_ack_segment(recv_stream);
            int acked_seg_index = cur_seg;
            while(segments[acked_seg_index].seq != ack_seg.ack) acked_seg_index++;
            //Check for corruption
            if (is_ack_corrupt(&ack_seg) || recv_res != 9) continue;
            if(markers[acked_seg_index].state = ACKED) //Duplicate
            {
                printf("Received duplicate ack %d.\n", ack_seg.ack);
                if (state == CONGESTION_AVOIDANCE || state == SLOW_START)
                {
                    dup_ack_count++;
                    if (dup_ack_count == 3)
                    {
                        ssthresh = cwnd/2;
                        cwnd = ssthresh + 3*MSS;
                        _transmit_segment(segments+cur_seg, markers+cur_seg, plp, pep, socket, dest, dest_len);
                        state = FAST_RECOVERY;
                    }
                }
                else //FAST_RECOVERY
                {
                    cwnd += MSS;
                    _transmit_cwnd(cwnd, segments, cur_seg, no_of_segments, markers, plp, pep, socket, dest, dest_len);
                }
            }
            else //Not duplicate
            {
                //If it's not a retransmit then use it to update estimatedRTT
                if(markers[acked_seg_index].state == SENT)
                {
                    clock_t now = clock();
                    int sampleRTT = ((double)now-markers[acked_seg_index].last_sent) / CLOCKS_PER_SEC * 1000000;
                    estimatedRTT = 0.875 * estimatedRTT + sampleRTT*0.125;
                    devRTT = devRTT*0.75 + 0.25*(sampleRTT > estimatedRTT ? sampleRTT-estimatedRTT : estimatedRTT-sampleRTT);
                    timeout_usec = estimatedRTT + 4*devRTT;
                }

                //Update cur_seg to next unacked segment
                markers[acked_seg_index].state = ACKED;
                while(cur_seg < no_of_segments && markers[cur_seg].state == ACKED) cur_seg++;
                if (cur_seg == no_of_segments) break;
                dup_ack_count = 0;
                
                if(state == SLOW_START)
                {
                    cwnd += MSS;
                    _transmit_cwnd(cwnd, segments, cur_seg, no_of_segments, markers, plp, pep, socket, dest, dest_len);
                    if(cwnd >= ssthresh) state = CONGESTION_AVOIDANCE;
                }
                else if(state == CONGESTION_AVOIDANCE)
                {
                    cwnd += MSS*(MSS/cwnd);
                    _transmit_cwnd(cwnd, segments, cur_seg, no_of_segments, markers, plp, pep, socket, dest, dest_len);
                }
                else
                    cwnd += MSS;
            }
        }
    }
    //Received all acknowledgements

    //Set socket to block recv calls for sending SYN
    struct timeval tv = {};
    tv.tv_usec = timeout_usec;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv)) {
        perror("setsockopt");
        return -1;
    }

    //Send SYN segment
    // Segment init_seg = {};
    // init_seg.len = 0;
    // init_seg.type = SYN;
    // init_seg.seq = segments[no_of_segments-1].seq;
    // init_seg.checksum = compute_checksum(&init_seg);

    //Send FIN segment
    Segment fin = {};
    fin.type = FIN;
    fin.len = 0;
    fin.checksum = compute_checksum(&fin);
    while(1)
    {
        printf("Sending FIN segment...\n");
        send_res = _transmit_segment(&fin, NULL, plp, pep, socket, dest, dest_len);
        if (send_res == -1)
        {
            perror("sending SYN");
            return -1;
        }
        else if (send_res != 0)
        {
            printf("Didn't send all of FIN segment. No action is taken\n");
        }
        recv_res = recvfrom(socket, recv_stream, ACK_LEN, 0, dest, dest_len);
        if (recv_res < 0)
        {
            if(errno == EWOULDBLOCK || errno == EAGAIN)
            {
                printf("Timeout. Resending FIN segment...\n");
                continue;
            }
            else{
                perror("recv FIN ACK");
                return -1;
            }
        }
        else 
        {
            ACK_Segment ack = to_ack_segment(recv_stream);
            if (ack.type != FIN || is_ack_corrupt(&ack))
            {
                printf("Corrupted ACK. Resending FIN segment...\n");
                continue;
            }
            else break;
        }
    }

    printf("Finished rdt_send().\n");
    return 0;
}

int _transmit_cwnd(int cwnd, Segment segments[], int cur_seg, int no_of_segments, Marker markers[], float plp, float pep, SOCKET s, struct sockaddr* dest, int dest_len)
{
    int final_seg = cur_seg;
    int sum = segments[cur_seg].len;
    while(final_seg != no_of_segments-1)
    {
        sum += segments[final_seg+1].len;
        if (sum < cwnd) final_seg++;
        else break;
    }
    for(int i=cur_seg; i<=final_seg; i++)
    {
        if (markers[i].state == ACKED) continue;
        int res = _transmit_segment(segments+i, markers+i, plp, pep, s, dest, dest_len);
        if (res < segments[i].len + SEGMENT_HEADER_LEN)
        {
            return res;
        }
    }
    return 0;
}

int _transmit_segment(Segment* seg, Marker* seg_mark, float plp, float pep, SOCKET s, struct sockaddr* dest, int dest_len)
{
    if (seg_mark != NULL)
    {
        switch(seg_mark->state)
        {
            case UNSENT:
                seg_mark->state = SENT;
            break;
            case SENT:
            case RESENT:
                seg_mark->state = RESENT;
            break;
        }
        seg_mark->last_sent = clock();
    }
    char will_lose = rand() % 100 < plp*100;
    int res;
    if (!will_lose)
    {
        char* stream = to_stream(seg);
        char will_corrupt = rand() % 100 < pep*100;
        if (will_corrupt)
        {
            stream[rand()%(seg->len)] += 1;
        }
        res = sendto(s, stream, seg->len+SEGMENT_HEADER_LEN, 0, dest, dest_len);
        free(stream);
    }
    else res = seg->len + SEGMENT_HEADER_LEN;
    return res;
}

//Reliable function for receiving data from a socket using a TCP-like policy.
int rdt_recv(SOCKET socket, char* buffer, int len, struct sockaddr_in *dest_addr, int* dest_addr_len, float plp, float pep)
{
    //infinite loop reading segments
        //read stream
        //if timeout and gotten SYN then send ACK 0
        //if timeout and haven't SYN then repeat
        //if haven't gotten SYN then
            //if corrupted or not SYN then repeat
            //else 
                //store last seq number
                //store expected seq as 0
                //send SYN ACK
                //mark SYN as received
                //continue
        //if gotten SYN
            //if corrupted then send send ACK [expected seq]
            //if not corrupted then check seq
                //if not expected seq then send ACK [expected seq]
                //else 
                    //transfer segment payload buffer
                    //increase expected seq and buffer head by segment.len
                    //Send ack [expected seq]



    return 0;
}

