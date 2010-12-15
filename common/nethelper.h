/*
 * This file is part of rdp2tcp
 *
 * Copyright (C) 2010-2011, Nicolas Collignon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __NETHELPER_H__
#define __NETHELPER_H__

#include "compiler.h"
#include "iobuf.h"

#define NETERR_RESOLVE -1
#define NETERR_NOADDR  -2
#define NETERR_SOCKET  -3
#define NETERR_BIND    -4
#define NETERR_LISTEN  -5
#define NETERR_CONNECT -6
#define NETERR_RECV    -7
#define NETERR_SEND    -8

#define NETERR_CLOSED  -1000

#ifndef _WIN32
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef int sock_t;
#define net_init()   ((void)0)
#define net_exit()   ((void)0)
#define net_close(x) close(x)
#define net_pending() ((errno == EINPROGRESS) || (errno == EAGAIN))
#define valid_sock(s) ((s) && (*(s) != -1))

#else
#include <winsock2.h>
#include <ws2tcpip.h>

typedef struct {
	SOCKET fd;
	WSAEVENT evt;
} sock_t;

void net_init(void);
#define net_exit()    WSACleanup()
void net_close(sock_t *);
#define net_pending() (WSAGetLastError() == WSAEWOULDBLOCK)
#define valid_sock(s) ((s) && ((s)->fd != INVALID_SOCKET) \
								&& ((s)->evt != WSA_INVALID_EVENT))

int net_update_watch(sock_t *, iobuf_t *);
#endif

#ifndef NETBUF_MAX_SIZE
#define NETBUF_MAX_SIZE (1024*16)
#endif

/** peer address */
typedef union {
	struct sockaddr_in  ip4; /**< IPv4 address */
	struct sockaddr_in6 ip6; /**< IPv6 address */
	unsigned int pid;        /**< process identifier */
} netaddr_t;

#define netaddr_af(na) (na)->ip4.sin_family
void netaddr_set(int, const void *, unsigned short, netaddr_t *);

int netaddr_cmp(const netaddr_t *, const netaddr_t *);
#define NETADDRSTR_MAXSIZE (1+INET6_ADDRSTRLEN+1+1+5+1)
const char *netaddr_print(const netaddr_t *, char *);

const char *net_error(int, int);

int net_resolve(int, const char *, unsigned short, netaddr_t *, int *);
int net_server(int, const char *, unsigned short, sock_t *, netaddr_t *,int*);
int net_client(int, const char *, unsigned short, sock_t *, netaddr_t *,int*);
int net_accept(sock_t *, sock_t *, netaddr_t *);
int net_read(sock_t*, iobuf_t*, unsigned int, unsigned int*, unsigned int*);
int net_write(sock_t *, iobuf_t *, const void *, unsigned int, unsigned int *);

#endif
