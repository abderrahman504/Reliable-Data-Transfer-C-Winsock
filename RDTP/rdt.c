#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <stdlib.h>
#include "Segment.c"
#include <math.h>


#define TIMEOUT_USEC 100000;

enum {UNSENT, SENT, RESENT, ACKED};
enum {SLOW_START, CONGESTION_AVOIDANCE, FAST_RECOVERY};

typedef struct {
    char state;
    clock_t last_sent;
} Marker;

int _transmit_cwnd(int , Segment*, int , int , Marker* , float , float , SOCKET , struct sockaddr* , int);
int _transmit_segment(Segment*, Marker*, float, float, SOCKET, struct sockaddr*, int);
int _transmit_data_ack(int, Segment*, float, float, SOCKET, struct sockaddr*, int);
int _transmit_ack(ACK_Segment*, float, float, SOCKET, struct sockaddr*, int);

//Reliable function for sending data through a socket using a TCP-like policy.
//Assumes selective acknowledgement
int rdt_send(SOCKET socket, char* buffer, int len, float plp, float pep, struct sockaddr *dest, int dest_len)
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

    // Change socket to blocking mode
    u_long mode = 0;  // 1 to enable non-blocking mode, 0 to disable
    if (ioctlsocket(socket, FIONBIO, &mode) != NO_ERROR) {
        printf("ioctlsocket failed with error: %u\n", WSAGetLastError());
        return -1;
    }
    //Set socket to block recv calls for sending SYN
    struct timeval tv = {};
    tv.tv_usec = TIMEOUT_USEC;
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
        else if (send_res != SEGMENT_HEADER_LEN)
        {
            printf("Didn't send all of SYN segment. No action is taken\n");
        }
        recv_res = recvfrom(socket, recv_stream, ACK_LEN, 0, dest, &dest_len);
        if (recv_res < 0)
        {
            printf("Timeout. Resending SYN segment...\n");
            continue;
            // if(errno == EWOULDBLOCK || errno == EAGAIN)
            // {
            // }
            // else{
            //     perror("recv SYN ACK");
            //     return -1;
            // }
        }
        else 
        {
            ACK_Segment ack = to_ack_segment(recv_stream);
            if (ack.type != SYN || is_ack_corrupt(&ack))
            {
                printf("Corrupted ACK. Resending SYN segment...\n");
                continue;
            }
            else 
            {
                printf("Received SYN ACK. Moving on...\n");
            }
            break;
        }
    }

    // Change socket to non-blocking mode
    mode = 1;  // 1 to enable non-blocking mode, 0 to disable
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
    int send_base = 0;
    int dup_ack_count = 0;
    int state = SLOW_START;
    Marker markers[no_of_segments];
    for(int i=0; i<no_of_segments; i++)
        markers[i].state = UNSENT;

    //Send segments
    printf("Transmitting cwnd...\n");
    send_res = _transmit_cwnd(cwnd, segments, send_base, no_of_segments, markers, plp, pep, socket, dest, dest_len);
    if (send_res < 0)
    {
        perror("sending cwnd");
        return -1;
    }
    else if (send_res != 0)
    {
        printf("Didn't send all of cwnd. No action is taken\n");
    }
    while(send_base < no_of_segments)
    {
        recv_res = recvfrom(socket, recv_stream, ACK_LEN, 0, dest, &dest_len);
        if (recv_res < 0)
        {
            clock_t now = clock();
            int time_msec = ((double)now-markers[send_base].last_sent) / CLOCKS_PER_SEC * 1000;
            if (time_msec >= timeout_usec) //timer expired
            {
                printf("Timeout.\n");
                ssthresh = cwnd / 2;
                cwnd = MSS;
                dup_ack_count = 0;
                time_msec *= 2;
                _transmit_segment(segments+send_base, markers+send_base, plp, pep, socket, dest, dest_len);
                state = SLOW_START;
            }
            continue;
            // if(WSAGetLastError() == EWOULDBLOCK || WSAGetLastError() == EAGAIN) //received nothing
            // {
            // }
            // else{
            //     perror("recv ACK");
            //     return -1;
            // }
        }
        else //received something
        {
            ACK_Segment ack_seg = to_ack_segment(recv_stream);
            //Check for corruption
            if (is_ack_corrupt(&ack_seg) || recv_res != 9) continue;
            if(ack_seg.ack < segments[send_base].seq+segments[send_base].len) //Duplicate
            {
                printf("Received duplicate ack %d.\n", ack_seg.ack);
                if (state == CONGESTION_AVOIDANCE || state == SLOW_START)
                {
                    dup_ack_count++;
                    if (dup_ack_count == 3)
                    {
                        ssthresh = cwnd/2;
                        cwnd = ssthresh + 3*MSS;
                        _transmit_segment(segments+send_base, markers+send_base, plp, pep, socket, dest, dest_len);
                        state = FAST_RECOVERY;
                    }
                }
                else //FAST_RECOVERY
                {
                    cwnd += MSS;
                    _transmit_cwnd(cwnd, segments, send_base, no_of_segments, markers, plp, pep, socket, dest, dest_len);
                }
            }
            else //Not duplicate
            {
                int cum_ack = send_base;
                while(segments[cum_ack].seq+segments[cum_ack].len != ack_seg.ack)
                    cum_ack++;
                //cum_ack will point to the last segment that was acked
                printf("Received new ack %d\n", ack_seg.ack);
                //If it's not a retransmit then use it to update estimatedRTT
                if(markers[cum_ack].state == SENT)
                {
                    clock_t now = clock();
                    int sampleRTT = ((double)now-markers[cum_ack].last_sent) / CLOCKS_PER_SEC * 1000000;
                    estimatedRTT = 0.875 * estimatedRTT + sampleRTT*0.125;
                    devRTT = devRTT*0.75 + 0.25*(sampleRTT > estimatedRTT ? sampleRTT-estimatedRTT : estimatedRTT-sampleRTT);
                    timeout_usec = estimatedRTT + 4*devRTT;
                }

                //Update send_base to next unacked segment
                int ack_count = cum_ack-send_base+1;
                for(int i=send_base; i<=cum_ack; i++) 
                    markers[i].state = ACKED;
                send_base = cum_ack+1;
                printf("send_base = %d\n", send_base);
                if (send_base == no_of_segments) break;
                dup_ack_count = 0;
                
                if(state == SLOW_START)
                {
                    cwnd += MSS*ack_count;
                    _transmit_cwnd(cwnd, segments, send_base, no_of_segments, markers, plp, pep, socket, dest, dest_len);
                    if(cwnd >= ssthresh) state = CONGESTION_AVOIDANCE;
                }
                else if(state == CONGESTION_AVOIDANCE)
                {
                    cwnd += MSS*(MSS/cwnd)*ack_count;
                    _transmit_cwnd(cwnd, segments, send_base, no_of_segments, markers, plp, pep, socket, dest, dest_len);
                }
                else
                {
                    cwnd = ssthresh;
                    state = CONGESTION_AVOIDANCE;
                }
            }
        }
    }
    //Received all acknowledgements

    // Change socket to blocking mode
    mode = 0;  // 1 to enable non-blocking mode, 0 to disable
    if (ioctlsocket(socket, FIONBIO, &mode) != NO_ERROR) {
        printf("ioctlsocket failed with error: %u\n", WSAGetLastError());
        return -1;
    }
    //Set socket to block recv calls for sending SYN
    struct timeval tv2 = {};
    tv.tv_usec = timeout_usec;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv2, sizeof tv2)) {
        perror("setsockopt");
        return -1;
    }

    //Send FIN segment
    Segment fin = {};
    fin.type = FIN;
    fin.seq = timeout_usec;
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

        recv_res = recvfrom(socket, recv_stream, ACK_LEN, 0, dest, &dest_len);
        if (recv_res < 0)
        {
            printf("Timeout. Resending FIN segment...\n");
            continue;
        }
        else 
        {
            ACK_Segment ack = to_ack_segment(recv_stream);
            if (ack.type != FIN || is_ack_corrupt(&ack))
            {
                printf("Corrupted ACK. Resending FIN segment...\n");
                continue;
            }
            
            else
            {
                printf("Received FIN ACK.\n");
                break;
            } 
        }
    }
    Sleep(100);
    printf("Finished rdt_send().\n");
    return 0;
}

int _transmit_cwnd(int cwnd, Segment* segments, int cur_seg, int no_of_segments, Marker* markers, float plp, float pep, SOCKET s, struct sockaddr* dest, int dest_len)
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
        printf("Transmitting seg# %d\n", i);
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
        printf("**SECRET** Segment corrupted!\n");
        }
        res = sendto(s, stream, seg->len+SEGMENT_HEADER_LEN, 0, dest, dest_len);
        free(stream);
    }
    else 
    {
        res = seg->len + SEGMENT_HEADER_LEN;
        printf("**SECRET** Segment lost!\n");
    }
    return res;
}

//Reliable function for receiving data from a socket using a TCP-like policy.
int rdt_recv(SOCKET socket, char* buffer, int len, float plp, float pep, struct sockaddr_in *dest_addr, int* dest_addr_len)
{
    if (dest_addr == NULL && dest_addr_len == NULL)
    {
        printf("dest_addr and dest_addr_len cannot be NULL\n");
        return -1;
    }
    char recv_stream[SEGMENT_HEADER_LEN + MSS];
    int recv_res;
    int send_res;

    // Change socket to blocking mode
    u_long mode = 0;  // 1 to enable non-blocking mode, 0 to disable
    if (ioctlsocket(socket, FIONBIO, &mode) != NO_ERROR) {
        printf("ioctlsocket failed with error: %u\n", WSAGetLastError());
        return -1;
    }
    //Set socket to block recv calls for sending SYN
    struct timeval tv = {};
    tv.tv_usec = TIMEOUT_USEC;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv)) {
        perror("setsockopt");
        return -1;
    }

    Segment* segments; //will allocate space for data segments when SYN is received.
    int no_of_segments;
    //Wait for SYN segment
    char syn_received = 0;
    Segment first_data = {};
    while(1)
    {
        if (syn_received) printf("Waiting for next segment...\n");
        else printf("Waiting for SYN segment...\n");
        
        recv_res = recvfrom(socket, recv_stream, SEGMENT_HEADER_LEN+MSS, 0, (struct sockaddr*)dest_addr, dest_addr_len);
        if (recv_res < 0)
        {
            printf("Timeout!\n");
            continue;
        }
        else 
        {
            printf("Received something\n");
            Segment seg = to_segment(recv_stream);
            if (is_corrupt(&seg))
            {
                printf("Corrupted SYN segment!\n");
                continue;
            }
            else
            {
                if (seg.type == SYN)
                {
                    printf("Received SYN segment.\n");
                    //Send SYN ACK
                    ACK_Segment ack = {};
                    ack.type = SYN;
                    ack.ack = 0;
                    ack.checksum = compute_ack_checksum(&ack);
                    printf("Sending SYN ACK...\n");
                    send_res = _transmit_ack(&ack, plp, pep, socket, (struct sockaddr*)dest_addr, *dest_addr_len);
                    if (send_res < 0)
                    {
                        perror("sending SYN ACK");
                        return -1;
                    }
                    else if (send_res != ACK_LEN)
                    {
                        printf("Didn't send all of SYN ACK. No action is taken\n");
                    }
                    if (!syn_received) //Allocate space for data segments
                    {
                        no_of_segments = (int)ceil((double)seg.seq/MSS) + 1;
                        segments = malloc(no_of_segments*sizeof(Segment));
                        
                    }
                    syn_received = 1;
                }
                else if (seg.type != SYN && !syn_received)
                {
                    printf("Received non-SYN segment before SYN. Ignoring...\n");
                    continue;
                }
                else
                {
                    printf("Received first data segment.\n");
                    first_data = to_segment(recv_stream);
                    break;
                }
            }
        }
    }

    // put first data where it belongs in segments array and send ack for awaited segment
    for (int i=0; i<no_of_segments; i++)
    {
        segments[i].type = 255;
    }
    int index = first_data.seq/MSS;
    segments[index] = first_data;
    int recv_base = index == 0 ? 1 : 0;
    printf("Sending ACK no#%d seq#%d...\n", recv_base-1, recv_base==0 ? 0 : segments[recv_base-1].seq+segments[recv_base-1].len);
    send_res = _transmit_data_ack(recv_base, segments, plp, pep, socket, (struct sockaddr*)dest_addr, *dest_addr_len);
    if (send_res < 0)
    {
        perror("sending data ACK");
        return -1;
    }
    else if (send_res != ACK_LEN)
    {
        printf("Didn't send all of data ACK. No action is taken\n");
    }

    Segment fin;
    int estimatedRTT;
    clock_t fin_last_sent;
    //Read data segments
    while(1)
    {
        printf("Waiting for next segment...\n");
        recv_res = recvfrom(socket, recv_stream, SEGMENT_HEADER_LEN+MSS, 0, (struct sockaddr*)dest_addr, dest_addr_len);
        if (recv_res < 0)
        {
            printf("Timeout!\n");
            continue;
        }
        else 
        {
            Segment seg = to_segment(recv_stream);
            if (is_corrupt(&seg))
            {
                printf("Corrupted data segment!\n");
            }
            else
            {
                if (seg.type == FIN)
                {
                    printf("Received FIN segment.\n");
                    fin = seg;
                    estimatedRTT = fin.seq;
                    //Send FIN ACK
                    ACK_Segment ack = {};
                    ack.type = FIN;
                    ack.ack = 0;
                    ack.checksum = compute_ack_checksum(&ack);
                    printf("Sending FIN ACK...\n");
                    fin_last_sent = clock();
                    send_res = _transmit_ack(&ack, plp, pep, socket, (struct sockaddr*)dest_addr, *dest_addr_len);
                    if (send_res < 0)
                    {
                        perror("sending FIN ACK");
                        return -1;
                    }
                    else if (send_res != ACK_LEN)
                    {
                        printf("Didn't send all of FIN ACK. No action is taken\n");
                    }
                    break;
                }
                else
                {
                    //store segment in buffer
                    index = seg.seq/MSS;
                    printf("Received segment no#%d seq#%d.\n", index, seg.seq);
                    segments[index] = seg;
                    //update recv_base
                    while(recv_base < no_of_segments)
                    {
                        if (segments[recv_base].type == 255) break;
                        recv_base++;
                    }
                }
            }
            //Send ACK for first unacked segment
            int seq = recv_base==0 ? 0 : segments[recv_base-1].seq+segments[recv_base-1].len;
            printf("Sending ACK no#%d seq#%d...\n", recv_base-1, seq);
            send_res = _transmit_data_ack(recv_base, segments, plp, pep, socket, (struct sockaddr*)dest_addr, *dest_addr_len);
            if (send_res < 0)
            {
                perror("sending data ACK");
                return -1;
            }
            else if (send_res != ACK_LEN)
            {
                printf("Didn't send all of data ACK. No action is taken\n");
            }
        }
    }

    // Change socket to non-blocking mode
    mode = 1;  // 1 to enable non-blocking mode, 0 to disable
    if (ioctlsocket(socket, FIONBIO, &mode) != NO_ERROR) {
        printf("ioctlsocket failed with error: %u\n", WSAGetLastError());
        return -1;
    }

    //Wait for timeout or another FIN segment
    while (1)
    {
        printf("Waiting to make sure sender received FIN ACK...\n");
        recv_res = recvfrom(socket, recv_stream, SEGMENT_HEADER_LEN+MSS, 0, (struct sockaddr*)dest_addr, dest_addr_len);
        if (recv_res < 0)
        {
            //check timer
            int time_usec = (double)(clock() - fin_last_sent) / CLOCKS_PER_SEC * 1000000;
            if (time_usec >= estimatedRTT)//Timer expired
            {
                printf("No response from sender. Terminating connection...\n");
                break;
            }
        }
        else 
        {
            Segment seg = to_segment(recv_stream);
            if (!is_corrupt(&seg) && seg.type != FIN)
            {
                printf("Received non-FIN segment. Terminating connection...\n");
                break;
            }
            printf("Received FIN segment.\n");
            //Send FIN ACK
            printf("Sending FIN ACK...\n");
            ACK_Segment ack = {};
            ack.type = FIN;
            ack.ack = 0;
            ack.checksum = compute_ack_checksum(&ack);
            fin_last_sent = clock();
            send_res = _transmit_ack(&ack, plp, pep, socket, (struct sockaddr*)dest_addr, *dest_addr_len);
            if (send_res < 0)
            {
                perror("sending FIN ACK");
                return -1;
            }
            else if (send_res != ACK_LEN)
            {
                printf("Didn't send all of FIN ACK. No action is taken\n");
            }
        }
    }
    
    int buffer_head = 0;
    for(int i=0; i<no_of_segments; i++)
    {
        Segment seg = segments[i];
        for(int j=0; j<segments[i].len; j++)
        {
            buffer[buffer_head] = seg.data[j];
            buffer_head++;
        }
    }

    printf("Finished rdt_recv()\n");
    free(segments);
    return buffer_head;
}

int _transmit_data_ack(int expected_seg_index, Segment* segments, float plp, float pep, SOCKET s, struct sockaddr* dest, int dest_len)
{
    ACK_Segment ack = {};
    ack.type = DATA;
    ack.ack = expected_seg_index == 0 ? 0 : segments[expected_seg_index-1].seq+segments[expected_seg_index-1].len;
    ack.checksum = compute_ack_checksum(&ack);
    return _transmit_ack(&ack, plp, pep, s, dest, dest_len);
}

int _transmit_ack(ACK_Segment* ack, float plp, float pep, SOCKET s, struct sockaddr* dest, int dest_len)
{
    char will_lose = rand() % 100 < plp*100;
    int res;
    if (!will_lose)
    {
        char* stream = ack_to_stream(ack);
        char will_corrupt = rand() % 100 < pep*100;
        if (will_corrupt)
        {
            stream[rand()%ACK_LEN] += 1;
            printf("**SECRET** ACK corrupted!\n");
        }
        res = sendto(s, stream, ACK_LEN, 0, dest, dest_len);
        free(stream);
    }
    else
    {
        res = ACK_LEN;
        printf("**SECRET** ACK lost!\n");
    }
    return res;
}

