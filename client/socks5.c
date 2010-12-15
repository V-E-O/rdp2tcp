/**
 * @file socks5.c
 * SOCKS5 server implementation
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
#include "socks5-proto.h"
#include "nethelper.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef DEBUG
extern int debug_level;
#endif

static int socks_error(netsock_t *cli, unsigned char ret)
{
	unsigned char out[2];

	out[0] = SOCKS5_VERSION;
	out[1] = ret;

	netsock_write(cli, out, 2);
	return -1;
}

/**
 * handle SOCKS5 server network accept-event
 * @param[in] cli client socket
 * @param[in] af client address family
 * @param[in] addr client address
 * @param[in] port client TCP port
 */
void socks5_connect_event(
					netsock_t *cli,
					int af,
					const void *addr,
					unsigned short port)
{
	unsigned int addr_len;
	unsigned char ans[4+16+2];

	assert(valid_netsock(cli) && (cli->type == NETSOCK_S5CLI)
			&& addr && ((af == AF_INET) || (af == AF_INET6)));
	trace_socks("");

	if (cli->state != NETSTATE_CONNECTING) {
		// server went wrong ...
		error("invalid SOCKS5 protocol state");
		tunnel_close(cli, 1);
		return;
	}

	ans[0] = SOCKS5_VERSION;
	ans[1] = SOCKS5_SUCCESS;
	ans[2] = 0;
	if (af == AF_INET) {
		ans[3] = SOCKS5_ATYPE_IPV4;
		addr_len = 4;
	} else {
		ans[3] = SOCKS5_ATYPE_IPV6;
		addr_len = 16;
	}

	memcpy(&ans[4], addr, addr_len);
	ans[4+addr_len] = (unsigned char) (port >> 8);
	ans[5+addr_len] = (unsigned char) (port & 0xff);

	cli->state = NETSTATE_CONNECTED;

	if (netsock_write(cli, ans, addr_len+6) >= 0) {

		if (iobuf_datalen(&cli->u.sockscli.ibuf) > 0) {
			if (channel_forward_iobuf(&cli->u.sockscli.ibuf,
												cli->tid) < 0) {
				tunnel_close(cli, 1);
			}
		}
	} else {
		// cancel tunnel
		tunnel_close(cli, 1);
	}
}

static int socks5_setup(netsock_t *cli)
{
	unsigned int len, methods_count, port_off;
	unsigned short port;
	unsigned char tunaf, tid, *buf, out[2];
	iobuf_t *ibuf;
	char *host, ip[INET6_ADDRSTRLEN+1];

	ibuf = &cli->u.sockscli.ibuf;

	if (netsock_read(cli, ibuf, 0, NULL) < 0)
		return -1;

#ifdef DEBUG
	if (debug_level > 2) iobuf_dump(ibuf);
#endif

	len = iobuf_datalen(ibuf);
	if (!len) // need more data
		return 1;

	buf = iobuf_dataptr(ibuf);
	if (buf[0] != SOCKS5_VERSION)
		return error("SOCKS5 protocol version not supported (0x%02x)", buf[0]);

	if (cli->state == NETSTATE_AUTHENTICATING) {

		if (len < 2) 
			return 1;

		methods_count = (unsigned int) buf[1];
		if (!methods_count)
			return error("no SOCKS authentication method proposed");

		if (methods_count + 2 > len) // need more data
			return 1;

		if (!memchr(buf+2, SOCKS5_NOAUTH, methods_count)) {
			// no valid auth
			return error("SOCKS5 authentication not supported");
		}

		iobuf_consume(ibuf, methods_count+2);
		out[0] = 5;
		out[1] = SOCKS5_NOAUTH;
		netsock_write(cli, &out, 2);
		cli->state = NETSTATE_AUTHENTICATED;
		debug(0, "SOCKS5 client authenticated");
		return 0;
	}

	if (cli->state != NETSTATE_AUTHENTICATED)
		return error("invalid SOCKS5 protocol state 0x%02x", cli->state);

	// +----+-----+-------+------+----------+----------+
	// |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
	// +----+-----+-------+------+----------+----------+
	// | 1  |  1  | X'00' |  1   | Variable |    2     |
	// +----+-----+-------+------+----------+----------+

	if (len < 8) // need more data
		return 1;

	if (buf[2] != 0)
		return error("invalid SOCKS5 reserved field (0x%02x)", buf[2]);

	if (buf[1] != SOCKS5_CONNECT) {
		warn("unsupported SOCKS5 command 0x%02x", buf[1]);
		return socks_error(cli, SOCKS5_UNKCOMMAND);
	}

	host = ip;
	ip[0] = 0;

	switch (buf[3]) {

		case SOCKS5_ATYPE_IPV4:
			if (len < 10)
				return 1;
			tunaf = TUNAF_IPV4;
			if (!inet_ntop(AF_INET, buf+4, ip, sizeof(ip)-1))
				return error("failed to convert SOCKS5 IPv4 address");
			port_off = 8;
			break;

		case SOCKS5_ATYPE_FQDN:
			if (len < 7+(unsigned int)buf[4])
				return 1;
			
			tunaf = TUNAF_ANY;
			len = (unsigned int) buf[4];
			host = malloc(len+1);
			if (!host)
				return error("failed to allocate SOCKS5 hostname");
			memcpy(host, buf+5, len);
			host[len] = 0;
			if (host && !*host) {
				if (host) free(host);
				return error("empty SOCKS5 domain");
			}
			port_off = 5 + (unsigned int) buf[4];
			break;

		case SOCKS5_ATYPE_IPV6:
			if (len < 22)
				return 1;
			tunaf = TUNAF_IPV6;
			if (!inet_ntop(AF_INET6, buf+4, ip, sizeof(ip)-1))
				return error("failed to convert SOCKS5 IPv6 address");
			port_off = 20;
			break;

		default:
			return socks_error(cli, SOCKS5_UNKADDRTYPE);
	}

	port = ntohs((((unsigned short)buf[port_off+1]) << 8) | buf[port_off]);
	if (!port) {
		if (host && (host != ip))
			free(host);
		return error("invalid SOCKS5 port");
	}
	iobuf_consume(ibuf, port_off+2);

	info(0, "SOCKS5 forward request to %s:%hu", host, port);

	tid = channel_request_tunnel(tunaf, host, port, 0);
	if (host && (host != ip))
		free(host);

	if (tid == 0xff)
		return -1;

	cli->tid   = tid;
	cli->state = NETSTATE_CONNECTING;

	return 0;
}

/**
 * handle SOCKS5 client network read-event
 * @param[in] cli client socket
 * @return 0 on success
 */
int socks5_read_event(netsock_t *cli)
{
	assert(valid_netsock(cli) && (cli->type == NETSOCK_S5CLI));
	trace_socks("state=0x%02x", cli->state);

	if (cli->state != NETSTATE_CONNECTED)
		return socks5_setup(cli);

	return channel_forward_recv(cli);
}

/**
 * handle SOCKS5 server network accept-event
 * @param[in] srv server socket
 * @return 0 on success
 */
void socks5_accept_event(netsock_t *srv)
{
	netsock_t *cli;
	char host[NETADDRSTR_MAXSIZE];

	assert(valid_netsock(srv) && (srv->type == NETSOCK_S5SRV));
	trace_socks("");

	cli = netsock_accept(srv);
	if (cli) {
		info(0, "accepted socks5 client %s", netaddr_print(&cli->addr, host));
		if (channel_is_connected()) {
			cli->type  = NETSOCK_S5CLI;
			cli->tid   = 0xff;
			cli->state = NETSTATE_AUTHENTICATING;
			iobuf_init2(&cli->u.sockscli.ibuf, &cli->u.sockscli.obuf, "socks5");
		} else {
			error("channel not connected");
			netsock_close(cli);
		}
	}
}

/**
 * start a SOCKS5 server
 * @param[in] cli socket of client who requested server start
 * @param[in] host local server hostname or IP address
 * @param[in] port local TCP port
 */
int socks5_bind(netsock_t *cli, const char *host, unsigned short port)
{
	netsock_t *srv;

	assert(valid_netsock(cli) && (cli->type == NETSOCK_CTRLCLI)
			&& host && *host && port);
	trace_socks("host=%s, port=%hu", host, port);

	srv = netsock_bind(cli, host, port, 0);
	if (!srv)
		return 0; // soft-error
	srv->type = NETSOCK_S5SRV;

	return controller_answer(cli, "SOCKS5 server listening on %s:%hu", host, port);
}

