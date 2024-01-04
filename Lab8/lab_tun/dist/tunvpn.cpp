/*
 *  Lab problem set for INP course
 *  by Chun-Ying Huang <chuang@cs.nctu.edu.tw>
 *  License: GPLv2
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <netinet/icmp6.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>

#define NIPQUAD(m)	((unsigned char*) &(m))[0], ((unsigned char*) &(m))[1], ((unsigned char*) &(m))[2], ((unsigned char*) &(m))[3]
#define errquit(m)	{ perror(m); exit(-1); }

#define MYADDR		0x0a0000fe
#define ADDRBASE	0x0a00000a
#define	NETMASK		0xffffff00
#define BUFFER_SIZE 1500
struct Packet {
    uint32_t virtual_ip;
};

int
tun_alloc(char *dev) {
	struct ifreq ifr;
	int fd, err;
	if((fd = open("/dev/net/tun", O_RDWR)) < 0 )
		return -1;
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI;	/* IFF_TUN (L3), IFF_TAP (L2), IFF_NO_PI (w/ header) */
	if(dev && dev[0] != '\0') strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	if((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
		close(fd);
		return err;
	}
	if(dev) strcpy(dev, ifr.ifr_name);
	return fd;
}

int
ifreq_set_mtu(int fd, const char *dev, int mtu) {
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_mtu = mtu;
	if(dev) strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	return ioctl(fd, SIOCSIFMTU, &ifr);
}

int
ifreq_get_flag(int fd, const char *dev, short *flag) {
	int err;
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	if(dev) strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	err = ioctl(fd, SIOCGIFFLAGS, &ifr);
	if(err == 0) {
		*flag = ifr.ifr_flags;
	}
	return err;
}

int
ifreq_set_flag(int fd, const char *dev, short flag) {
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	if(dev) strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	ifr.ifr_flags = flag;
	return ioctl(fd, SIOCSIFFLAGS, &ifr);
}

int
ifreq_set_sockaddr(int fd, const char *dev, int cmd, unsigned int addr) {
	struct ifreq ifr;
	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	memset(&ifr, 0, sizeof(ifr));
	memcpy(&ifr.ifr_addr, &sin, sizeof(struct sockaddr));
	if(dev) strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	return ioctl(fd, cmd, &ifr);
}

int
ifreq_set_addr(int fd, const char *dev, unsigned int addr) {
	return ifreq_set_sockaddr(fd, dev, SIOCSIFADDR, addr);
}

int
ifreq_set_netmask(int fd, const char *dev, unsigned int addr) {
	return ifreq_set_sockaddr(fd, dev, SIOCSIFNETMASK, addr);
}

int
ifreq_set_broadcast(int fd, const char *dev, unsigned int addr) {
	return ifreq_set_sockaddr(fd, dev, SIOCSIFBRDADDR, addr);
}

int
tunvpn_server(int port) {
	// XXX: implement your server codes here ...
	fprintf(stderr, "## [server] starts ...\n");
	while(1) { 
		char buffer[BUFFER_SIZE] = "tun0";
		int tun_fd = tun_alloc(buffer);
		if(tun_fd == -1){
			return -1;
		}
		short flag;
		int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP), MTU = 1400;
		ifreq_get_flag(sock, buffer, &flag);
		ifreq_set_flag(sock, buffer, (flag | IFF_UP));
		ifreq_set_mtu(sock, buffer, MTU);
		ifreq_set_addr(sock, buffer, htonl(MYADDR));
		ifreq_set_netmask(sock, buffer, htonl(NETMASK));
		ifreq_set_broadcast(sock, buffer, htonl(MYADDR | ~NETMASK));
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		struct hostent *he = gethostbyname("server");
    	if (he == NULL) errquit("gethostbyname");
    	addr.sin_addr = *(struct in_addr *)he->h_addr;
    	printf("Server address: %u.%u.%u.%u\n", NIPQUAD(addr.sin_addr.s_addr));
    	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) errquit("bind");
		struct Packet packet;
    	std::map<uint32_t, struct sockaddr_in> client_addr;
		fd_set readset;
		int maxfd = std::max(sock, tun_fd);
		memset(buffer, 0, sizeof(buffer));
		uint32_t client_num = 0;
		while(true){
			FD_ZERO(&readset);
        	FD_SET(tun_fd, &readset);
        	FD_SET(sock, &readset);
        	int nready = select(maxfd + 1, &readset, NULL, NULL, NULL);
        	if (nready < 0) errquit("select");
			if(FD_ISSET(sock, &readset)){
				socklen_t addrlen = sizeof(addr);
            	ssize_t size = recvfrom(sock, &buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &addrlen);
				printf("Received UDP packet from client\n");
				if(size == sizeof(packet)){
					memcpy(&packet, buffer, sizeof(packet));
					if(packet.virtual_ip == 0){
						packet.virtual_ip = htonl(ADDRBASE + client_num);
						sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&addr, sizeof(addr));
						printf("Assign virtual IP: %u.%u.%u.%u to client %d\n", NIPQUAD(packet.virtual_ip), client_num + 1);
						client_addr[packet.virtual_ip] = addr;
						client_num++;
						printf("Client address table:\n");
						for (auto it = client_addr.begin(); it != client_addr.end(); it++) {
							printf("Virtual IP: %u.%u.%u.%u\n", NIPQUAD(it->first));
							printf("Client address: %u.%u.%u.%u\n", NIPQUAD(it->second.sin_addr.s_addr));
						}
						printf("Client actual address: %u.%u.%u.%u\n", NIPQUAD(addr.sin_addr.s_addr));
                    	continue;
					}
					else printf("Error: virtual IP is not 0\n");
					continue;
				}
				struct iphdr *ip = (struct iphdr *)buffer;
				if(ip->daddr == htonl(MYADDR)){
					printf("Send packet to tun0\n");
					write(tun_fd, buffer, size);
					continue;
				}
				auto it = client_addr.find(ip->daddr);
				if (it == client_addr.end()) {
					printf("Error: cannot find the virtual IP\n");
					continue;
				}
				struct sockaddr_in client = it->second;
				printf("Send packet to IP: %u.%u.%u.%u\n", NIPQUAD(it->first));
				size = sendto(sock, buffer, size, 0, (struct sockaddr *)&client, sizeof(client));
			}
			if(FD_ISSET(tun_fd, &readset)){
				printf("Received packet from tun0\n");
				ssize_t n = read(tun_fd, buffer, sizeof(buffer));
				struct iphdr* ip = (struct iphdr *)buffer;
				uint32_t virtual_ip = ip->daddr;
				auto it = client_addr.find(virtual_ip);
				if (it == client_addr.end()) {
					printf("Error: cannot find the virtual IP\n");
					continue;
				}
				struct sockaddr_in client = it->second;
				printf("Send packet to IP: %u.%u.%u.%u\n", NIPQUAD(it->first));
				n = sendto(sock, buffer, n, 0, (struct sockaddr *)&client, sizeof(client));
				if (n < 0) errquit("sendto");
			}
		}

	}
	return 0;
}

int
tunvpn_client(const char *server, int port) {
	// XXX: implement your client codes here ...
	fprintf(stderr, "## [client] starts ...\n");
	while(1) { 
		int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    	if (sock < 0) errquit("socket");

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		struct hostent *he = gethostbyname("server");
		if (he == NULL) errquit("gethostbyname");
		addr.sin_addr = *(struct in_addr *)he->h_addr;
		printf("Server address: %u.%u.%u.%u\n", NIPQUAD(addr.sin_addr.s_addr));
		if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) errquit("connect");
		struct Packet config;
		config.virtual_ip = 0;
		printf("Send config packet to server\n");
    	int n = sendto(sock, &config, sizeof(config), 0, (struct sockaddr *)&addr, sizeof(addr));
    	if (n < 0) errquit("sendto");
		printf("Receiving config packet from server...\n");
		socklen_t addrlen = sizeof(addr);
		n = recvfrom(sock, &config, sizeof(config), 0, (struct sockaddr *)&addr, &addrlen);
		if (n < 0) errquit("recvfrom");
		printf("Recv config packet from server\n");
		char tun_name[IFNAMSIZ] = "tun0";
		int32_t tun_fd = tun_alloc(tun_name);
		int16_t flag;
		uint32_t virtual_ip = config.virtual_ip;
		int32_t broadcast_ip = htonl(MYADDR + 1);
		int32_t netmask = htonl(NETMASK);
		int32_t mtu = 1400;
		if (tun_fd < 0) errquit("tun_alloc");
		ifreq_set_mtu(sock, "tun0", mtu);
		ifreq_get_flag(sock, "tun0", &flag);
		ifreq_set_flag(sock, "tun0", flag | IFF_UP | IFF_RUNNING);
		ifreq_set_addr(sock, "tun0", virtual_ip);
		ifreq_set_netmask(sock, "tun0", netmask);
		ifreq_set_broadcast(sock, "tun0", broadcast_ip);
		printf("Virtual IP: %u.%u.%u.%u\n", NIPQUAD(virtual_ip));
		
		fd_set readset;
		int maxfd = std::max(sock, tun_fd);
		char buffer[BUFFER_SIZE];
		memset(buffer, 0, BUFFER_SIZE);

		while(true){
			FD_ZERO(&readset);
			FD_SET(tun_fd, &readset);
			FD_SET(sock, &readset);
			int nready = select(maxfd+1, &readset, NULL, NULL, NULL);
			if (nready < 0) errquit("select");

			if(FD_ISSET(sock, &readset)){
				//printf("Recv UDP packet from server\n");
				socklen_t addrlen = sizeof(addr);
				ssize_t n = recvfrom(sock, &buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &addrlen);
            	if (n < 0) errquit("UDP recvfrom");
				struct iphdr *ip = (struct iphdr *)buffer;
				// printf("IP header: ");
            	// printf("saddr: %u.%u.%u.%u, ", NIPQUAD(ip->saddr));
            	// printf("daddr: %u.%u.%u.%u\n", NIPQUAD(ip->daddr));
				if (ip->daddr != virtual_ip) {
					printf("Error: virtual IP is not correct\n");
					printf("Virtual IP: %u.%u.%u.%u\n", NIPQUAD(virtual_ip));
					continue;
				}
				// send packet to tun0
				n = write(tun_fd, buffer, n);
				if (n < 0) errquit("tun write");
			}
			if(FD_ISSET(tun_fd, &readset)){
				// printf("Recv packet from tun0\n");

				ssize_t n = read(tun_fd, buffer, BUFFER_SIZE);
				if(n < 0) errquit("tun read");

				// struct iphdr *ip = (struct iphdr *)buffer;
				// printf("IP header: ");
				// printf("tos: %d, ", ip->tos);
				// printf("tot_len: %d, ", ip->tot_len);
				// printf("id: %d, ", ip->id);
				// printf("frag_off: %d, ", ip->frag_off);
				// printf("ttl: %d, ", ip->ttl);
				// printf("protocol: %d, ", ip->protocol);
				// printf("check: %d, ", ip->check);
				// printf("saddr: %u.%u.%u.%u, ", NIPQUAD(ip->saddr));
				// printf("daddr: %u.%u.%u.%u\n", NIPQUAD(ip->daddr));

				// send the buffer to server｀
				n = sendto(sock, buffer, n, 0, (struct sockaddr *)&addr, sizeof(addr));
				if (n < 0) errquit("sendto");
			}
		}
	}

	return 0;
}

int
usage(const char *progname) {
	fprintf(stderr, "usage: %s {server|client} {options ...}\n"
		"# server mode:\n"
		"	%s server port\n"
		"# client mode:\n"
		"	%s client servername serverport\n",
		progname, progname, progname);
	return -1;
}

int main(int argc, char *argv[]) {
	if(argc < 3) {
		return usage(argv[0]);
	}
	if(strcmp(argv[1], "server") == 0) {
		if(argc < 3) return usage(argv[0]);
		return tunvpn_server(strtol(argv[2], NULL, 0));
	} else if(strcmp(argv[1], "client") == 0) {
		if(argc < 4) return usage(argv[0]);
		return tunvpn_client(argv[2], strtol(argv[3], NULL, 0));
	} else {
		fprintf(stderr , "## unknown mode %s\n", argv[1]);
	}
	return 0;
}
