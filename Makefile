CC=gcc
CFLAGS=-Wall -pedantic
TCP_SERVER_SOURCE=tcpServer.c server.h storage.c storage.h
TCP_PROXY_SOURCE=tcpProxy.c server.h
UDP_SERVER_SOURCE=udpServer.c server.h storage.c storage.h
UDP_PROXY_SOURCE=udpProxy.c

all: tcpServer tcpProxy udpServer udpProxy

tcpServer: $(TCP_SERVER_SOURCE)
	$(CC) $(CFLAGS) -o tcpServer $(TCP_SERVER_SOURCE)

tcpProxy: $(TCP_PROXY_SOURCE)
	$(CC) $(CFLAGS) -o tcpProxy $(TCP_PROXY_SOURCE)

udpServer: $(UDP_SERVER_SOURCE)
	$(CC) $(CFLAGS) -o udpServer $(UDP_SERVER_SOURCE)

udpProxy: $(UDP_PROXY_SOURCE)
	$(CC) $(CFLAGS) -o udpProxy $(UDP_PROXY_SOURCE)

clean:
	rm -f *.o tcpServer tcpProxy udpServer udpProxy