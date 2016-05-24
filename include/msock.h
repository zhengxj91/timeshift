#ifndef MSOCK_H
#define MSOCK_H

/* this is for windows compat */
#define SOCKET int

#ifdef __MINGW32__ 
#undef SOCKET
#undef socklen_t 
#define WINVER 0x0501 
#include <ws2tcpip.h> 
#define EWOULDBLOCK WSAEWOULDBLOCK
#define close closesocket
#define socklen_t int
typedef unsigned int in_addr_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

/* Define IPV6_ADD_MEMBERSHIP for FreeBSD and Mac OS X */
#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#endif

/**
 Creates a socket suitable for sending unicast/multicast datagrams via sendto().

 On success, a socket is returned and unicast/multicastAddr set with the right info for sendto() calls.
 On error, -1 is returned.
 */
SOCKET InitTransmitterSocket(char* addr, char* port, char* iface, struct addrinfo** multicastAddr);

#endif
