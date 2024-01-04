
CC		= gcc
PROGS	= server
UID		= $(shell stat -c "%u:%g" /build/Makefile 2>/dev/null || stat -c "%u:%g" ./Makefile 2> /dev/null)

all: server

%.o: %.c
	$(CC) -c -g -Wall $<

server: server.o
	$(CC) -o server -O3 -flto -static $^ -lpthread -lssl -lcrypto
	chown -R "$(UID)" *.o server

https:
	$(CC) -o https_server -O3 -flto -static server.c -lpthread -lssl -lcrypto

clean:
	rm -f *.o $(PROGS)
