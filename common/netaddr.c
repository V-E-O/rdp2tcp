/**
 * @file netaddr.c
 * netaddr_t structure helpers
 */
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
#include "nethelper.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <arpa/inet.h>
#endif

/**
 * setup a netaddr_t structure
 * @param[in] af address family (AF_INET or AF_INET6)
 * @param[in] addr address in binary format
 * @param[in] port service port 
 * @param[out] a the netaddr_t structure to setup
 */
void netaddr_set(int af, const void *addr, unsigned short port, netaddr_t *a)
{
	assert(((af == AF_INET) || (af == AF_INET6)) && addr && port && a);

	if (af == AF_INET) {
		a->ip4.sin_family = AF_INET;
		a->ip4.sin_port = ntohs(port);
		memcpy(&a->ip4.sin_addr, addr, 4);
	} else {
		a->ip6.sin6_family = AF_INET;
		a->ip6.sin6_port = ntohs(port);
		memcpy(&a->ip6.sin6_addr, addr, 16);
	}
}

/**
 * compare 2 netaddr_t structures
 * @return 0 if both structures are the same
 */
int netaddr_cmp(const netaddr_t *a, const netaddr_t *b)
{
	assert(a && ((netaddr_af(a) == AF_INET) || (netaddr_af(a) == AF_INET6))
			&& b && ((netaddr_af(b) == AF_INET) || (netaddr_af(b) == AF_INET6)));

	if (netaddr_af(a) != netaddr_af(b))
		return 1;

	if (netaddr_af(a) == AF_INET) {

		if (((struct sockaddr_in*)a)->sin_port
				!= ((struct sockaddr_in*)b)->sin_port)
			return 1;


		return ((struct sockaddr_in*)a)->sin_addr.s_addr
				!= ((struct sockaddr_in*)b)->sin_addr.s_addr;
	}


	if (((struct sockaddr_in6*)a)->sin6_port
			!= ((struct sockaddr_in6*)b)->sin6_port)
		return 1;

	return memcmp(&((struct sockaddr_in6*)a)->sin6_addr,
						&((struct sockaddr_in6*)b)->sin6_addr, 16);
}

/**
 * convert a netaddr_t structure to a string
 * @param[in] addr netaddr_t structure to serialize
 * @param[out] buf output string buffer
 * @return a pointer to buf
 * @note buf size must be at least NETADDRSTR_MAXSIZE
 */
const char *netaddr_print(const netaddr_t *addr, char *buf)
{
	unsigned short port;
	char *ptr;
#ifndef _WIN32
	void *a;
#else
	DWORD len, addr_len;
#endif

	assert(buf && addr);
	if ((netaddr_af(addr) != AF_INET) && (netaddr_af(addr) != AF_INET6))
		return (const char*)memcpy(buf, "???", 4);

	ptr = buf;
	//memset(buf, 0, NETADDRSTR_MAXSIZE);

	if (netaddr_af(addr) == AF_INET) {
#ifndef _WIN32
		a = (void *)&addr->ip4.sin_addr;
#else
		addr_len = sizeof(struct sockaddr_in);
#endif
		port = addr->ip4.sin_port;
	} else {
		*ptr++ = '[';
#ifndef _WIN32
		a = (void *)&addr->ip6.sin6_addr;
#else
		addr_len = sizeof(struct sockaddr_in6);
#endif
		port = addr->ip6.sin6_port;
	}

#ifndef _WIN32
	if (inet_ntop(netaddr_af(addr), a, ptr, INET6_ADDRSTRLEN))
		ptr += strlen(ptr);
	else
		*ptr++ = '?';
#else
	len = INET6_ADDRSTRLEN;
	ptr[0] = 0;
	if (!WSAAddressToStringA((struct sockaddr *)addr, addr_len, NULL, ptr, &len))
		ptr += len;
	else
		*ptr++ = '?';
#endif

	if (netaddr_af(addr) == AF_INET6)
		*ptr++ = ']';

	snprintf(ptr, 7, ":%hu", ntohs(port));

	return (const char*) buf;
}

