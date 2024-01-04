#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#define PORT 12345
#define BUFFER_SIZE 32768

int cmpfunc (const void * a, const void * b){
   return ( *(double*)a - *(double*)b );
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    // char message[5];
	// for(int i=0;i<5;i++)message[i] = 'a';

    // Create socket file descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation error");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        return -1;
    }
    double latency=0;
    struct timespec start, end;
    char msg[] = "Abccccc";
    //char msgget[1];
    
    //send(sock, msg, sizeof(msg), 0);
    //sleep(1);
    //for(int i=0;i<1;i++){
    clock_gettime(CLOCK_MONOTONIC, &start);
    //sendto(sock, msg, sizeof(msg), 0);
    recv(sock, msg, sizeof(msg), 0);
    clock_gettime(CLOCK_MONOTONIC, &end);
    latency = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
        //printf("%f\n", d);
        //latency += d;
    //}
    //latency -= 5;
    latency/=2;
    //if(latency > 15) latency -= 3;
    latency = round(latency);
    //if(latency > 50) latency -= 3;
    //printf("%ld, %ld\n", end.tv_usec, start.tv_usec);
    
    //latency = ((end.tv_sec - start.tv_sec) * 1000  + ((end.tv_usec - start.tv_usec)/10000)*10);

    double bandwidth = 0;
    double bws[64];
    for(int i=0;i<64;i++){

        clock_gettime(CLOCK_MONOTONIC, &start);

        int bytesRead = recv(sock, buffer, sizeof(buffer), 0);
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        double delay = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
        bytesRead *= 8; 
        double tempbw = bytesRead / (delay*1000);
        // if(tempbw > 300) tempbw -= 2;
        // if(tempbw > 400) tempbw -= 3;
        // if(tempbw > 500) tempbw -= 30;
        // if(tempbw < 100) tempbw += 1;

        //if(delay < latency/2){
        // bandwidth = p ? (tempbw + bandwidth * p) / (p + 1) : tempbw;
        // p++;
        bws[i] = tempbw;
        //printf("recv time = %lf ms, byterecv = %d bytes, bandwidth = %lf Mbps\n", delay, bytesRead, tempbw);
        //}
        // gettimeofday(&start, NULL);
    }
    close(sock);
    qsort(bws, 64, sizeof(double), cmpfunc);
    bandwidth = (bws[32] + bws[33])/2;
    // for(int i=0;i<64;i++) {
    //     printf("%f\n", bws[i]);
    //     if(bws[i] > bandwidth + 2.5 || bws[i] < bandwidth - 2.5){bandwidth+=bws[i]; p++;}
    // }
    // bandwidth /= (p+1);
    
    printf("# RESULTS: delay = %.2f ms, bandwidth = %.2f Mbps\n", latency, round(bandwidth));
    return 0;
}
