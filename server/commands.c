/**
 * @file commands.c
 * rdp2tcp commands handling
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
#include "rdp2tcp.h"
#include "r2twin.h"
#include "msgparser.h"

static int protoerror(unsigned char tid, unsigned char err, const char *errstr)
{
	channel_write(R2TCMD_CONN, tid, &err, 1);
	return error("protocol error (%s)", errstr);
}

static int start_tcp_tunnel(
					const r2tmsg_connreq_t *msg,
					unsigned int len,
					int bind_tunnel)
{
	static const int r2taf_to_sysaf[3] = { AF_UNSPEC, AF_INET, AF_INET6 };

	if (len < 7)
		return protoerror(msg->id, R2TERR_BADMSG, "command too small");

	if (tunnel_lookup(msg->id))
		return error("tunnel 0x%02x is already used", msg->id);

	if (msg->af > TUNAF_IPV6)
		return protoerror(msg->id, R2TERR_BADMSG, "invalid address family");

	if (msg->hostname[len-6])
		return protoerror(msg->id, R2TERR_BADMSG, "invalid hostname");

	tunnel_create(msg->id, r2taf_to_sysaf[msg->af],
						msg->hostname, ntohs(msg->port), bind_tunnel);

	return 0;
}
	
static int cmd_conn(const r2tmsg_connreq_t *msg, unsigned int len)
{
	trace_chan("len=%u, tid=0x%02x, af=0x%02x, port=0x%04x",
		len, msg->id, msg->af, msg->port);

	return start_tcp_tunnel(msg, len, 0);
}

static int cmd_bind(const r2tmsg_connreq_t *msg, unsigned int len)
{
	trace_chan("len=%u, tid=0x%02x, af=0x%02x, port=0x%04x",
		len, msg->id, msg->af, msg->port);

	return start_tcp_tunnel(msg, len, 1);
}

static int cmd_close(const r2tmsg_t *msg, unsigned int len)
{
	tunnel_t *tun;
	
	trace_chan("len=%u, tid=0x%02x", len, msg->id);
	tun = tunnel_lookup(msg->id);
	if (!tun) {
		error("invalid tunnel id 0x%02x", msg->id);
		return 0;
	}

	tunnel_close(tun);
	return 0;
}

static int cmd_data(const r2tmsg_t *msg, unsigned int len)
{
	tunnel_t *tun;
	
	trace_chan("len=%u, id=0x%02x", len, msg->id);
	tun = tunnel_lookup(msg->id);
	if (!tun) {
		error("invalid tunnel id 0x%02x", msg->id);
		return 0;
	}

	return tunnel_write(tun, ((const char *)msg)+2, len-2);
}

const cmdhandler_t cmd_handlers[R2TCMD_MAX] = {
	(cmdhandler_t) cmd_conn,  /* R2TCMD_CONN */
	(cmdhandler_t) cmd_close, /* R2TCMD_CLOSE */
	(cmdhandler_t) cmd_data,  /* R2TCMD_DATA */
	NULL,
	(cmdhandler_t) cmd_bind,  /* R2TCMD_BIND */
	NULL
};

