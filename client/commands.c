/**
 * @file commands.c
 * rdp2tcp commands handlers
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
#include "msgparser.h"

#include <arpa/inet.h>

extern const char *r2t_errors[R2TERR_MAX];

static int badproto(netsock_t *cli)
{
	assert(valid_netsock(cli));

	tunnel_close(cli, 0);
	return error("bad server protocol");
}

static netsock_t *check_tunnel_id(const r2tmsg_t *msg)
{
	netsock_t *ns;

	ns = tunnel_lookup(msg->id);
	if (!ns) {
		warn("unknown tunnel 0x%02x", msg->id);
		channel_close_tunnel(msg->id);
	}

	return ns;
}

static int check_binding_answer(
					int mode,
					const r2tmsg_connans_t *msg,
					unsigned int len)
{
	netsock_t *cli;
	int af;
	unsigned short port;

	assert(msg && (len >= 3));
	trace_chan("len=%u, tid=%u, err=%u", len, msg->id, msg->err);

	cli = check_tunnel_id((const r2tmsg_t*)msg);
	if (!cli)
		return 0;

	if ((mode == 2) || (msg->err == 0)) {
		if (len < 8)
			return badproto(cli);

		port = ntohs(msg->port);

		switch (msg->af) {

			case TUNAF_ANY:
				if (mode)
					return badproto(cli);
				// process tunnel
				if (len != 10)
					return badproto(cli);
				af = AF_UNSPEC;
				break;

			case TUNAF_IPV4:
				if (len != 10)
					return badproto(cli);
				af = AF_INET;
				break;

			case TUNAF_IPV6:
				if (len != 22)
					return badproto(cli);
				af = AF_INET6;
				break;
			default:
				return badproto(cli);
		}

		if (mode != 2) {
			if (cli->type == NETSOCK_TUNCLI)
				tunnel_connect_event(cli, af, &msg->addr[0], port);
			else if (cli->type == NETSOCK_S5CLI)
				socks5_connect_event(cli, af, &msg->addr[0], port);
			else
				tunnel_bind_event(cli, af, &msg->addr[0], port);

		} else {

			if (!tunnel_lookup(msg->err)) {
				tunnel_revconnect_event(cli, msg->err, af, &msg->addr[0], port);
			} else {
				// server allocated an already used tunnel ID
				channel_close_tunnel(msg->err);
			}
		}

	} else {
		error("failed to %s tunnel 0x%02x (%s)",
			(mode ? "bind" : "connect"),
			cli->tid,
			(msg->err >= R2TERR_MAX ? "???" : r2t_errors[msg->err]));
		tunnel_close(cli, 0);
	}

	return 0;
}

static int cmd_conn(const r2tmsg_t *msg, unsigned int len)
{
	return check_binding_answer(0, (const r2tmsg_connans_t *)msg, len);
}

static int cmd_bind(const r2tmsg_t *msg, unsigned int len)
{
	return check_binding_answer(1, (const r2tmsg_connans_t *)msg, len);
}

static int cmd_close(const r2tmsg_t *msg, unsigned int len)
{
	netsock_t *tun;

	assert(msg && (len >= 2));
	trace_chan("len=%u", len);

	tun = check_tunnel_id(msg);
	if (tun)
		netsock_cancel(tun);

	return 0;
}

static int cmd_data(const r2tmsg_t *msg, unsigned int len)
{
	netsock_t *clitun;

	assert(msg && (len >= 3));
	trace_chan("len=%u", len);

	clitun = check_tunnel_id(msg);
	if (!clitun)
		return 0;

	return tunnel_write(clitun, ((const char *)msg)+2, len-2);
}

static int cmd_ping(const r2tmsg_t *msg, unsigned int len)
{
	assert(msg && (len >= 2));
	//trace_chan("len=%u", len);

	channel_pong();
	return 0;
}

static int cmd_rconn(const r2tmsg_t *msg, unsigned int len)
{
	return check_binding_answer(2, (const r2tmsg_connans_t *)msg, len);
}

/**
 * handlers for each command
 */
const cmdhandler_t cmd_handlers[R2TCMD_MAX] = {
	cmd_conn,  // R2TCMD_CONN
	cmd_close, // R2TCMD_CLOSE
	cmd_data,  // R2TCMD_DATA
	cmd_ping,  // R2TCMD_PING
	cmd_bind,  // R2TCMD_BIND
	cmd_rconn  // R2TCMD_RCONN
};

