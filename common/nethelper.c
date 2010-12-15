/**
 * @file nethelper.c
 * network client/server helpers
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
#ifndef _WIN32
#include <string.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#ifndef _WIN32
#define nethelper_error errno
#define nethelper_badsock -1
#define close_sock(x) close(x)
#define net_fd(s) (*(s))

#else
#define nethelper_error WSAGetLastError()
#define nethelper_badsock INVALID_SOCKET
#define close_sock(x) closesocket(x)
#define ENOMEM ERROR_NOT_ENOUGH_MEMORY
#define net_fd(s) ((s)->fd)

/**
 * initialize network subsystem
 */
void net_init(void)
{
	WSADATA wsa;
	WSAStartup(MAKEWORD(2,2), &wsa);
}

/**
 * close socket and associated event
 */
void net_close(sock_t *s)
{
	closesocket(s->fd);
	WSACloseEvent(s->evt);
}

/**
 * update socket events filter
 * @param[in] s the socket
 * @param[in] obuf output buffer associated with the socket
 * @return 0 on success
 */
int net_update_watch(sock_t *s, iobuf_t *obuf)
{
	assert(valid_sock(s) && valid_iobuf(obuf));
	return WSAEventSelect(s->fd, s->evt,
								 (iobuf_datalen(obuf) > 0 
								  	? FD_READ|FD_WRITE|FD_CLOSE
									: FD_READ|FD_CLOSE));
}
#endif // _WIN32

/**
 * return a string describing the error
 * @return the error description
 * @note the returned string is hold in a static buffer
 */
const char *net_error(int ret, int err)
{
	const char *x;
	static char buffer[512];
#ifdef _WIN32
	static char msg[512];
#endif
	static const char *actions_errors[] = {
		"failed to resolve hostname", 
		"no valid address", 
		"failed to create socket", 
		"failed to bind socket", 
		"failed to setup socket", 
		"failed to connect", 
		"failed to receive", 
		"failed to send"
	};

	x = ((ret >= NETERR_SEND) && (ret < 0)) ? actions_errors[-ret-1] : "???";
	
#ifndef _WIN32
	snprintf(buffer, sizeof(buffer)-1, "%s (%s)", x,
				(ret == NETERR_RESOLVE ? gai_strerror(err) : strerror(err)));
#else
	msg[0] = 0;
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM
						|FORMAT_MESSAGE_IGNORE_INSERTS
						|FORMAT_MESSAGE_MAX_WIDTH_MASK,
						NULL, err,
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						(LPSTR)msg, sizeof(msg), NULL);
	snprintf(buffer, sizeof(buffer)-1, "%s (%s)", x, msg);
#endif
	return (const char *) buffer;
}

static int netres(
					int mode,
					int pref_af,
					const char *host,
					unsigned short port,
					sock_t *out_sock,
					netaddr_t *addr,
					int *err)
{
#ifndef _WIN32
	int fd;
#else
	SOCKET fd;
	WSAEVENT evt;
#endif
	int ret, n;
	struct addrinfo hints, *res, *ptr;
	char service[8];

	assert(((pref_af==AF_UNSPEC) || (pref_af==AF_INET) || (pref_af==AF_INET6))
			&& host && *host && port && addr && err && (out_sock || !mode));
	*err = 0;

	if (addr)
		memset(addr, 0, sizeof(*addr));

	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = pref_af;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(service, sizeof(service)-1, "%hu", port);

	res = NULL;
	ret = getaddrinfo(host, service, &hints, &res);
	if (ret) {
		*err = ret;
		return NETERR_RESOLVE;
	}

	ret = NETERR_NOADDR;
	fd  = nethelper_badsock;
#ifdef _WIN32
	evt = WSA_INVALID_EVENT;
#endif

	// for each hostname resolution result
	for (ptr=res; ptr; ptr=ptr->ai_next) {

		if (addr)
			memcpy(addr, ptr->ai_addr, ptr->ai_addrlen);

		if (!mode) { // resolve-only
			ret = 0;
			break;
		}

		// create new async socket
		fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (fd == nethelper_badsock) {
			*err = nethelper_error;
			ret = NETERR_SOCKET;
			break;
		}

#ifndef _WIN32
		fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)|O_NONBLOCK);
#else
		evt = WSACreateEvent();
		if (evt == WSA_INVALID_EVENT) {
			*err = nethelper_error;
			ret = NETERR_SOCKET;
			break;
		}
#endif

		if (mode == 1) {
			// tcp server
			n = 1;
			setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,(const void*)&n, sizeof(n));

			if (!bind(fd, ptr->ai_addr, ptr->ai_addrlen)) {

				if (!listen(fd, 5)) {
#ifdef _WIN32
					if (WSAEventSelect(fd, evt, FD_ACCEPT)) {
						*err = nethelper_error;
						ret = NETERR_SOCKET;
						break;
					}
#endif
					ret = 0;
				} else {
					*err = nethelper_error;
					ret = NETERR_LISTEN;
				}
				break;
			}
			ret = NETERR_BIND;

		} else {
			// tcp client
#ifdef _WIN32
			if (WSAEventSelect(fd, evt, FD_CONNECT|FD_CLOSE)) {
				*err = nethelper_error;
				ret = NETERR_SOCKET;
				break;
			}
#endif
			if (!connect(fd, ptr->ai_addr, ptr->ai_addrlen)) {
#ifdef _WIN32
				if (WSAEventSelect(fd, evt, FD_READ|FD_CLOSE)) {
					*err = nethelper_error;
					ret = NETERR_SOCKET;
					break;
				}
#endif
				ret = 0;
				break;
			}
			if (net_pending()) {
				ret = 1;
				break;
			}
			*err = nethelper_error;
			ret = NETERR_CONNECT;
		}

#ifdef _WIN32
		WSACloseEvent(evt);
#endif
		close_sock(fd);
		*err = nethelper_error;
	}

	freeaddrinfo(res);

	if ((ret >= 0) && mode) {
#ifndef _WIN32
		*out_sock = fd;
#else
		out_sock->fd = fd;
		out_sock->evt = evt;
#endif
	} else if (fd != nethelper_badsock) {
		close_sock(fd);
#ifdef _WIN32
		if (evt != WSA_INVALID_EVENT)
			WSACloseEvent(evt);
#endif
	}

	return ret;
}

/**
 * resolve a hostname
 * @return -1 on error, 0 on success
 */
int net_resolve(
		int pref_af,
		const char *host,
		unsigned short port,
		netaddr_t *addr,
		int *err)
{

	return netres(0, pref_af, host, port, NULL, addr, err);
}

/**
 * resolve a hostname and bind a socket server
 * @return -1 on error, 0 on success
 */
int net_server(
		int pref_af,
		const char *host,
		unsigned short port,
		sock_t *out_sock,
		netaddr_t *addr,
		int *err)
{
	return netres(1, pref_af, host, port, out_sock, addr, err);
}

/**
 * resolve a hostname and connect a socket client
 * @return -1 on error, 0 on success, 1 if connection is pending
 */
int net_client(
		int pref_af,
		const char *host,
		unsigned short port,
		sock_t *out_sock,
		netaddr_t *addr,
		int *err)
{

	return netres(2, pref_af, host, port, out_sock, addr, err);
}

/**
 * accept a client connection
 * @param[in] srv the server socket
 * @param[out] cli client socket
 * @param[out] addr client network address
 * @return 0 on success
 */
int net_accept(sock_t *srv, sock_t *cli, netaddr_t *addr)
{
	socklen_t addrlen;

	assert(valid_sock(srv) && cli && addr);
	addrlen = sizeof(*addr);

#if defined(HAVE_ACCEPT4)
	*cli = accept4(*srv, (struct sockaddr *)addr, &addrlen, SOCK_NONBLOCK);
	if (*cli == nethelper_badsock)
		return nethelper_error;

#elif !defined(_WIN32)
	*cli = accept(*srv, (struct sockaddr *)addr, &addrlen);
	if (*cli == nethelper_badsock)
		return nethelper_error;
	fcntl(*cli, F_SETFL, fcntl(*cli, F_GETFL)|O_NONBLOCK);

#else
	cli->fd = accept(srv->fd, (struct sockaddr *)addr, &addrlen);
	if (cli->fd == nethelper_badsock)
		return nethelper_error;

	cli->evt = WSACreateEvent();
	if (cli->evt == WSA_INVALID_EVENT) {
		return nethelper_error;
	}
	if (WSAEventSelect(cli->fd, cli->evt, FD_READ|FD_CLOSE)) {
		WSACloseEvent(cli->evt);
		return nethelper_error;
	}
#endif

	return 0;
}

/**
 * async read from file descriptor to I/O buffer
 * @param[in] s socket
 * @param[in,out] ibuf input buffer
 * @param[in] prefix_size allocation head padding size
 * @param[in,out] min_size minimal I/O chunk size
 * @param[out] out_size hold transfer size on success
 * @return -1 on error, 0 on success and 1 if the operation would block
 */
int net_read(
			sock_t *s,
			iobuf_t *ibuf,
			unsigned int prefix_size,
			unsigned int *min_size,
			unsigned int *out_size)
{
	ssize_t ret;
	unsigned int avail, curr_min_size;
	char *buf;

	assert(valid_sock(s) && valid_iobuf(ibuf) && out_size);

	if (min_size && !*min_size)
		*min_size = IOBUF_MIN_SIZE;

	curr_min_size = (min_size ? *min_size : IOBUF_MIN_SIZE);

	*out_size = 0;

	buf = iobuf_reserve(ibuf, curr_min_size, &avail);
	if (!buf)
		return -ENOMEM;
	assert(avail > prefix_size);

#ifndef _WIN32
	ret = read(net_fd(s), buf+prefix_size, avail-prefix_size);
#else
	ret = recv(net_fd(s), buf+prefix_size, avail-prefix_size, 0);
#endif
	if (ret > 0) {
		iobuf_commit(ibuf, prefix_size + (unsigned int) ret);
		*out_size = (unsigned int) ret;

		if (ret == (avail - prefix_size)) {
			 // increase I/O chunks size (perfs..)
			curr_min_size <<= 1;
			if (curr_min_size > NETBUF_MAX_SIZE)
				curr_min_size = NETBUF_MAX_SIZE;
			*min_size = curr_min_size;
		}
		return 0;
	}

	if (!ret)
		return NETERR_CLOSED;

	if (net_pending())
		return 1;
	
	return -(int)nethelper_error;
}

/**
 * async write from I/O buffer to file descriptor
 * @param[in] s socket
 * @param[in] obuf I/O output buffer
 * @param[in] data data to append to I/O buffer
 * @param[in] size size of data to append
 * @param[out] out_size hold transfer size on success
 * @return -1 on error, 0 on success and 1 if the operation would block
 */
int net_write(
			sock_t *s,
			iobuf_t *obuf,
			const void *data,
			unsigned int size,
			unsigned int *out_size)
{
	ssize_t ret;
	unsigned int used;

	assert(valid_sock(s) && valid_iobuf(obuf ) && (data || !size) && out_size);

	*out_size = 0;
	used = iobuf_datalen(obuf);

	if (size > 0) {
		if (!used) {
			// try zero-copy send
#ifndef _WIN32
			ret = write(net_fd(s), data, size);
#else
			ret = send(net_fd(s), data, size, 0);
#endif
			if (ret < 0)
				return net_pending() ? 1 : -(int)nethelper_error;

			if (!ret)
				return NETERR_CLOSED;

			data = ((const char *)data) + ret;
			size -= ret;
			*out_size = (unsigned int) ret;
			if (!size) {
#ifdef _WIN32
				// don't watch FD_WRITE events anymore
				if (WSAEventSelect(s->fd, s->evt, FD_READ|FD_CLOSE))
					return -(int)nethelper_error;
#endif
				return 0;
			}
		}

		if (!iobuf_append(obuf, data, size))
			return -ENOMEM;

		return 1;
	}

	if (!used)
		return 0;

#ifndef _WIN32
	ret = write(net_fd(s), iobuf_dataptr(obuf), used);
#else
	ret = send(net_fd(s), iobuf_dataptr(obuf), used, 0);
#endif
	if (ret < 0)
		return net_pending() ? 1 : -(int)nethelper_error;

	if (!ret)
		return NETERR_CLOSED;

	iobuf_consume(obuf, (unsigned int) ret);
	*out_size = (unsigned int) ret;

#ifdef _WIN32
	if (used == (unsigned int)ret) {
		// don't watch FD_WRITE events anymore
		if (WSAEventSelect(s->fd, s->evt, FD_READ|FD_CLOSE))
			return -(int)nethelper_error;
	}
#endif
	return 0;
}
