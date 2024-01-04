#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>


#define PORT 12345
#define BUFFER_SIZE 2097152

int main(int argc, char *argv[]) {
    int connfd;
    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);
	int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);
    
    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
			perror("listen");
			exit(EXIT_FAILURE);
	}

	while(1){
        if ((connfd = accept(server_fd, (struct sockaddr *)&cliaddr, &clilen)) < 0) {
            perror("server accept fail\n");
            return 0;
        }

        // ----- delay -------
        // char buf[32768];
        // recv(connfd, buf, sizeof(buf), 0);
        // send(connfd, buf, sizeof(buf), 0);
        char msg[] = "Hello";
        send(connfd, msg, sizeof(msg), 0);


        char mes[BUFFER_SIZE];
        memset(mes, 'a', sizeof(mes));
        for(int i=0;i<2;i++)
        send(connfd, mes, sizeof(mes), 0);

	}
    
    close(server_fd);
    return 0;
}
