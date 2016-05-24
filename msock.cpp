#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "msock.h"

SOCKET InitTransmitterSocket(char* addr, char* port, char* interface, struct addrinfo **dst_Addr) {

	int is_multicast_addr = 1;
	SOCKET sock;
	struct addrinfo hints = { 0 }; /* Hints for name lookup */

#ifdef WIN32
	WSADATA trash;
	if(WSAStartup(MAKEWORD(2,0),&trash)!=0)
	DieWithError("Couldn't init Windows Sockets\n");
#endif

	/*
	 Resolve destination address for udp datagrams
	 */
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICHOST;
	int status;

	struct in_addr out_ip;

	inet_aton(addr, &out_ip);
	is_multicast_addr = IN_MULTICAST(ntohl(out_ip.s_addr));
	if (is_multicast_addr) {
		fprintf(stderr, "		--> Multicast Dst: %s:%s, Interface: %s. \n",
				addr, port,
				interface ? interface : "default");
	} else
		fprintf(stderr, "		--> Unicast Dst: %s:%s, Interface: %s. \n",
				addr, port,
				interface ? interface : "default");

	if ((status = getaddrinfo(addr, port, &hints,
			dst_Addr)) != 0) {
		fprintf(stderr, "destination: %s\n", gai_strerror(status));
		return -1;
	}

	/*
	 Create socket for sending udp datagrams
	 */
	if ((sock = socket((*dst_Addr)->ai_family, (*dst_Addr)->ai_socktype, 0))
			< 0) {
		perror("socket() failed");
		freeaddrinfo(*dst_Addr);
		return -1;
	}

	if (is_multicast_addr) {
		/*
		 set the sending interface
		 */
		struct in_addr iface;
		if (interface)
			inet_aton(interface, &iface);
		else
			iface.s_addr = INADDR_ANY;
		if ((*dst_Addr)->ai_family == PF_INET) {
			if (setsockopt(sock,
			IPPROTO_IP,
			IP_MULTICAST_IF, (char*) &iface, sizeof(iface)) != 0) {
				perror("interface setsockopt() sending interface");
				freeaddrinfo(*dst_Addr);
				return -1;
			}
		}
		if ((*dst_Addr)->ai_family == PF_INET6) {
			unsigned int ifindex = 0; /* 0 means 'default interface'*/
			if (setsockopt(sock,
			IPPROTO_IPV6,
			IPV6_MULTICAST_IF, (char*) &ifindex, sizeof(ifindex)) != 0) {
				perror("interface setsockopt() sending interface");
				freeaddrinfo(*dst_Addr);
				return -1;
			}
		}
	}

	return sock;
}
