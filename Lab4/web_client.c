#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

int main(int argc, char *argv[]){
    char SERVER[] = "140.113.213.213";
    int PORT = 10314;
    //char SERVER[] = "172.21.0.4";
    //int PORT = 10001;


    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr(SERVER);
    
    // char getotp[512];
    // snprintf(getotp, sizeof(getotp), "GET /otp?addr HTTP/1.1\r\nHost: %s:%d\r\n\r\n", SERVER, PORT);
    // send(sockfd, getotp, strlen(getotp), 0);
    // char buf[2048];
    // memset(buf,'\0',sizeof(buf));
    // while(recv(sockfd, buf, sizeof(buf), 0)){
    //     for (int i = 0, j=0; buf[i] != '\0'; i++){
    //         if(i >= 155) token[j++] = buf[i];
    //     }
    // }
    // close(sockfd);

    int validotp = 0;
    char token[256];
    char dryrun[256];
    while(!validotp){
        // ---------- get otp --------------
        int sockfd1 = socket(AF_INET, SOCK_STREAM, 0);
        connect(sockfd1, (struct sockaddr*)&servaddr, sizeof(servaddr));

        char stuid[]  = "110652019";
        char getotp[512];
        snprintf(getotp, sizeof(getotp), "GET /otp?name=%s HTTP/1.1\r\nHost: %s:%d\r\n\r\n", stuid, SERVER, PORT);
        send(sockfd1, getotp, strlen(getotp), 0);
        usleep(300000);
        char buf[2048];
        memset(buf,'\0',sizeof(buf));
        bzero(token, sizeof(token)); 
        while(recv(sockfd1, buf, sizeof(buf), 0)){
            for (int i = 0, j=0; buf[i] != '\0'; i++){
                //printf("%c",buf[i]);
                if(i >= 155) token[j++] = buf[i];
            }
        }
        close(sockfd1);

        // ------------- verify ------------
        //printf("%s\n", token);
        usleep(50000);

        int sockfd2 = socket(AF_INET, SOCK_STREAM, 0);
        connect(sockfd2, (struct sockaddr*)&servaddr, sizeof(servaddr));
        char verify[1024];
        snprintf(verify, sizeof(verify), "GET /verify?otp=%s HTTP/1.1\r\nHost: %s:%d\r\n\r\n", token, SERVER, PORT);
        //printf("%s\n", verify);
        send(sockfd2, verify, strlen(verify), 0);
        usleep(300000);
        bzero(dryrun, sizeof(dryrun));
        memset(buf,'\0',sizeof(buf));
        while(recv(sockfd2, buf, sizeof(buf), 0)){
            for (int i = 0, j = 0; buf[i] != '\0'; i++){
                if(i >= 140) dryrun[j++] += buf[i];
            }
        }
        //printf("%s\n", dryrun);
        for(int i=0;i<sizeof(dryrun);i++){
            if(dryrun[i] == 'O' && dryrun[i+1] == 'K'){
                validotp = 1;
                break;
            }
        }
        close(sockfd2);
        usleep(1000000);
    }

    char filepath[] = "out.txt";
    // FILE *out = fopen(filepath, "w");
    // if (out == NULL) {
    //     perror("Error opening file");
    //     exit(EXIT_FAILURE);
    // }
    // fprintf(out, "%s", token);
    // fclose(out);
    

    int sockfd3 = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr(SERVER);
    connect(sockfd3, (struct sockaddr*)&servaddr, sizeof(servaddr));
    char boundary[] = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    char upload[2048];
    snprintf(upload, sizeof(upload), "POST /upload HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: multipart/form-data; boundary=%s\r\nContent-Length: 292\r\n\r\n--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=%s\r\nContent-Type: text/plain\r\n\r\n%s\r\n--%s--\r\n", SERVER, PORT, boundary, boundary, filepath, token, boundary);
    send(sockfd3, upload, strlen(upload), 0);
    usleep(300000);
    char buf[2048];
    memset(buf, '\0', sizeof(buf));
    while (recv(sockfd3, buf, sizeof(buf), 0)) {
        for (int i = 0; buf[i] != '\0'; i++)
            printf("%c", buf[i]);
    }
    printf("\n");
    close(sockfd3);

    return 0;
}
