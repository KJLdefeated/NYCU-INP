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

#define NIPQUAD(m)	((unsigned char*) &(m))[0], ((unsigned char*) &(m))[1], ((unsigned char*) &(m))[2], ((unsigned char*) &(m))[3]
#define errquit(m)	{ perror(m); exit(-1); }

#define MYADDR		0x0a0000fe
#define ADDRBASE	0x0a00000a
#define	NETMASK		0xffffff00

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
	while(1) { pause(); }
	return 0;
}

int
tunvpn_client(const char *server, int port) {
	// XXX: implement your client codes here ...
	fprintf(stderr, "## [client] starts ...\n");
	while(1) { pause(); }
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
