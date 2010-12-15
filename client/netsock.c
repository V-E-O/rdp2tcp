/**
 * @file netsock.c
 * network sockets management
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
#include "r2tcli.h"
#include "nethelper.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>

/**
 * all network sockets double-linked list
 */
LIST_HEAD_INIT(all_sockets);

/**
 * check if main loop must wait for network-write event
 * @param[in] ns netsock socket
 */
int netsock_want_write(netsock_t *ns)
{
	assert(valid_netsock(ns));

	switch (ns->type) {

		case NETSOCK_CTRLCLI:
			return iobuf_datalen(&ns->u.ctrlcli.obuf) > 0;

		case NETSOCK_TUNCLI:
		case NETSOCK_RTUNCLI:
			return (ns->state != NETSTATE_CONNECTED)
					|| (iobuf_datalen(&ns->u.tuncli.obuf) > 0);

		case NETSOCK_S5CLI:
			return iobuf_datalen(&ns->u.sockscli.obuf) > 0;
			//return (ns->u.sockscli.state < S5STATE_CONNECTED)
			//		|| (iobuf_datalen(&ns->u.sockscli.obuf) > 0);
	}

	return 0;
}

/**
 * cancel a network socket / delayed netsock_close
 * @param[in] ns netsock socket
 */
void netsock_cancel(netsock_t *ns)
{
	assert(valid_netsock(ns) && (ns->state != NETSTATE_CANCELLED));
	ns->state = NETSTATE_CANCELLED;
}

/**
 * close a network socket
 * @param[in] ns netsock socket
 */
void netsock_close(netsock_t *ns)
{
	assert(ns && (((ns->type == NETSOCK_UNDEF) || valid_netsock(ns))));

	list_del(&ns->list);

	if (ns->type != NETSOCK_RTUNSRV)
		close(ns->fd);

	switch (ns->type) {

		case NETSOCK_CTRLCLI:
			iobuf_kill2(&ns->u.ctrlcli.ibuf, &ns->u.ctrlcli.obuf);
			break;

		case NETSOCK_TUNCLI:
			iobuf_kill(&ns->u.tuncli.obuf);
			break;

		case NETSOCK_S5CLI:
			iobuf_kill2(&ns->u.sockscli.ibuf, &ns->u.sockscli.obuf);
			break;
	}

	free(ns);
}

/**
 * allocate a netsock_t structure
 * @param[in] cli caller socket
 * @param[in] fd socket
 * @param[in] addr associated socket address
 * @param[in] extra_size extra padding allocated for structure
 * @return allocated structure
 */
netsock_t *netsock_alloc(
					netsock_t *cli,
					int fd,
					netaddr_t *addr,
					unsigned int extra_size)
{
	netsock_t *ns;

	ns = calloc(1, sizeof(*ns)+extra_size);
	if (ns) {
		ns->type = NETSOCK_UNDEF;
		ns->type = NETSTATE_INIT;
		ns->tid  = 0xff;
		ns->fd = fd;
		if (addr)
			memcpy(&ns->addr, addr, sizeof(*addr));
		list_add_tail(&ns->list, &all_sockets);
	} else {
		error("failed to allocated socket structure");
		if (cli)
			controller_answer(cli, "failed to allocated socket structure");
		close(fd);
	}

	return ns;
}

/**
 * start a server socket
 * @param[in] cli caller socket
 * @param[in] host listening address 
 * @param[in] port listening port
 * @param[in] extra_size extra padding allocated for structure
 * @return allocated structure
 */
netsock_t *netsock_bind(
		netsock_t *cli,
		const char *host,
		unsigned short port,
		unsigned int extra_size)
{
	netsock_t *srv;
	int ret, err, fd;
	netaddr_t addr;

	assert((!cli || valid_netsock(cli)) && host && *host && port);

	ret = net_server(AF_UNSPEC, host, port, &fd, &addr, &err);
	if (ret < 0) {
		error("%s", net_error(ret, err));
		if (cli)
			controller_answer(cli, "error: %s", net_error(ret, err));
		return NULL;
	}

	srv = netsock_alloc(NULL, fd, &addr, extra_size);
	if (srv)
		srv->state = NETSTATE_CONNECTED;

	return srv;
}

/**
 * accept client socket
 * @param[in] srv server socket
 * @return allocated structure
 */
netsock_t *netsock_accept(netsock_t *srv)
{
	netsock_t *cli;
	int ret, fd;
	netaddr_t addr;

	assert(valid_netsock(srv));

	ret = net_accept(&srv->fd, &fd, &addr);
	if (ret) {
		error("failed to accept connection (%s)", strerror(ret));
		return NULL;
	}

	cli = netsock_alloc(NULL, fd, &addr, 0);
	if (cli)
		cli->state = NETSTATE_CONNECTED;

	return cli;
}

/**
 * start a client socket
 * @param[in] host client address 
 * @param[in] port client port
 * @return allocated structure
 */
netsock_t *netsock_connect(const char *host, unsigned short port)
{
	netsock_t *cli;
	int ret, err, fd;
	netaddr_t addr;

	assert(host && *host && port);

	ret = net_client(AF_UNSPEC, host, port, &fd, &addr, &err);
	if (ret < 0) {
		error("failed to connect to %s:%hu (%s)",
				host, port, net_error(ret, err));
		return NULL;
	}

	cli = netsock_alloc(NULL, fd, &addr, 0);
	if (cli)
		cli->state = (ret ? NETSTATE_CONNECTING : NETSTATE_CONNECTED);

	return cli;
}

/**
 * async read from socket
 * @param[in] ns network socket
 * @param[in] ibuf input buffer
 * @param[in] prefix_size head padding used for buffer allocation
 * @param[out] out_size total bytes transferred
 * @return -1 on error, 0 on success, 1 if pending
 */
int netsock_read(
			netsock_t *ns,
			iobuf_t *ibuf,
			unsigned int prefix_size,
			unsigned int *out_size)
{
	int ret;
	unsigned int r;
	char host[NETADDRSTR_MAXSIZE];

	assert(valid_netsock(ns) && ibuf);

	ret = net_read(&ns->fd, ibuf, prefix_size, &ns->min_io_size, &r);
	if (ret < 0) {
		netaddr_print(&ns->addr, host);
		if (ret == NETERR_CLOSED)
			info(0, "connection %s closed", host);
		else
			error("failed to recv data from %s (%s)", host, strerror(errno));

	} else if (r > 0) {
		if (out_size)
			*out_size = r;
		print_xfer("tcp", 'r', r);
	}

	return ret;
}

/**
 * async write to socket
 * @param[in] ns network socket
 * @param[in] buf optional data to send
 * @param[in] len buffer size
 * @return -1 on error, 0 on success, 1 if pending
 */
int netsock_write(netsock_t *ns, const void *buf, unsigned int len)
{
	int ret;
	unsigned int w;
	char host[NETADDRSTR_MAXSIZE];

	assert(valid_netsock(ns) && (buf || !len));

	ret = net_write(&ns->fd, &ns->u.tuncli.obuf, buf, len, &w);
	if (ret < 0) {
		netaddr_print(&ns->addr, host);
		if (ret == NETERR_CLOSED)
			info(0, "connection %s closed", host);
		else
			error("failed to send data to %s (%s)", host, strerror(errno));

	} else if (w > 0) {
		print_xfer("tcp", 'w', w);
	}

	return ret;
}

