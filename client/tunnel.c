/**
 * @file tunnel.c
 * rdp2tcp tunnel management
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

#include <string.h>
#include <errno.h>

extern struct list_head all_sockets;

/**
 * lookup socket by tunnel ID
 * @param[in] tid tunnel ID
 * @return NULL if socket was not found
 */
netsock_t *tunnel_lookup(unsigned char tid)
{
	netsock_t *ns;

	assert(tid != 0xff);
	trace_tun("id=0x%02x", tid);

	list_for_each(ns, &all_sockets) {
		if (ns->tid == tid)
			return ns;
	}

	return NULL;
}

static unsigned char last_tid = 0xff;

/**
 * generate a unused tunnel ID
 * @return 0xff on error (all tunnel ID are used)
 */
unsigned char tunnel_generate_id(void)
{
	unsigned char tid;

	for (tid=last_tid+1; tid!=last_tid; ++tid) {
		if (!tunnel_lookup(tid)) {
			last_tid = tid;
			return tid;
		}
	}

	error("failed to find available tunnel id");
	return 0xff;
}

static unsigned char sysaf_to_rdpaf(int af)
{
	switch (af) {
		case AF_INET: return TUNAF_IPV4;
		case AF_INET6: return TUNAF_IPV6;
	}
	return TUNAF_ANY;
}

/**
 * register a new TCP forwarding tunnel
 * @param[in] cli socket of the client who requested the tunnel
 * @param[in] lhost local hostname or IP address
 * @param[in] lport local TCP port
 * @param[in] raf remote address family (AF_INET/INET6/UNSPEC)
 * @param[in] rhost remote hostname
 * @param[in] rport remote TCP port
 * @return 0 or 1 if the controller is still connected
 */
int tunnel_add(
			netsock_t *cli,
			char *lhost,
			unsigned short lport,
			int raf,
			char *rhost,
			unsigned short rport)
{
	size_t rhost_len;
	netsock_t *ns;
	char str[NETADDRSTR_MAXSIZE*2 + 64];

	assert(valid_netsock(cli) && lhost && *lhost && lport && rhost && *rhost);
	trace_tun("%s:%hu --> %s:%hu", lhost, lport, rhost, rport);

	rhost_len = strlen(rhost) + 1;
	ns = netsock_bind(cli, lhost, lport, rhost_len);
	if (!ns) 
		return 0; // soft error, no need to kill client


	ns->type = NETSOCK_TUNSRV;
	ns->u.tunsrv.raf   = sysaf_to_rdpaf(raf);
	ns->u.tunsrv.rport = rport;
	memcpy(ns->u.tunsrv.rhost, rhost, rhost_len);

	if (rport) {
		snprintf(str, sizeof(str)-1, "tunnel [%s]:%hu --> [%s]:%hu registered",
					lhost, lport, rhost, rport);
	} else {
		snprintf(str, sizeof(str)-1, "tunnel [%s]:%hu --> %s registered",
					lhost, lport, rhost);
	}

	info(0, str);
	return controller_answer(cli, str);
}

/**
 * register a new reverse connect TCP tunnel
 * @param[in] cli socket of the client who requested the tunnel
 * @param[in] lhost local hostname or IP address
 * @param[in] lport local TCP port
 * @param[in] raf remote address family (AF_INET/INET6/UNSPEC)
 * @param[in] rhost remote hostname
 * @param[in] rport remote TCP port
 * @return 0 or 1 if the controller is still connected
 */
int tunnel_add_reverse(
			netsock_t *cli,
			char *lhost,
			unsigned short lport,
			int raf,
			char *rhost,
			unsigned short rport)
{
	size_t lhost_len, rhost_len;
	netsock_t *ns;
	char str[NETADDRSTR_MAXSIZE*2 + 64];

	assert(valid_netsock(cli) && lhost && *lhost && lport && rhost && *rhost);
	trace_tun("%s:%hu <-- %s:%hu", lhost, lport, rhost, rport);

	lhost_len = strlen(lhost) + 1;
	rhost_len = strlen(rhost) + 1;
	ns = netsock_alloc(cli, -1, NULL, lhost_len + rhost_len);
	if (!ns) 
		return 0; // soft-error .. maybe hard but dont kill client

	ns->type = NETSOCK_RTUNSRV;
	ns->u.rtunsrv.lport = lport;
	ns->u.rtunsrv.rport = rport;
	ns->u.rtunsrv.lhost_len = (unsigned short) lhost_len;
	memcpy(ns->u.rtunsrv.lhost, lhost, lhost_len);
	memcpy(&ns->u.rtunsrv.lhost[lhost_len], rhost, rhost_len);

	if (channel_is_connected()) {
		// request tunnel binding right now if channel is connected
		ns->tid = channel_request_tunnel(TUNAF_ANY, rhost, rport, 1);
		if (ns->tid == 0xff) {
			netsock_close(ns);
			return controller_answer(cli, "error: failed to request port binding");
		}
	}

	snprintf(str, sizeof(str)-1, "tunnel [%s]:%hu <-- [%s]:%hu is being registred",
				lhost, lport, rhost, rport);
	info(0, str);
	return controller_answer(cli, str);
}

/**
 * try to remove tunnel removal
 * @param[in] cli socket of client who requested tunnel removal
 * @param[in] lhost tunnel local hostname
 * @param[in] lport tunnel local TCP port
 * @return 0 or 1 if the controller is still connected
 */
int tunnel_del(netsock_t *cli, char *lhost, unsigned short lport)
{
	netsock_t *ns;
	int ret, err;
	netaddr_t addr;

	assert(valid_netsock(cli) && lhost && *lhost && lport);
	trace_tun("host=%s:%i", lhost, lport);

	ret = net_resolve(AF_UNSPEC, lhost, lport, &addr, &err);
	if (ret)
		return controller_answer(cli, "error: %s", net_error(ret, err));

	list_for_each(ns, &all_sockets) {

		ret = 1;

		switch (ns->type) {

			case NETSOCK_TUNSRV:
			case NETSOCK_S5SRV:
				ret = netaddr_cmp(&ns->addr, &addr);
				break;

			case NETSOCK_RTUNSRV:
				ret = ((lport != ns->u.rtunsrv.lport)
						|| strcmp(lhost, ns->u.rtunsrv.lhost));
				break;
		}

		if (!ret) {
			tunnel_close(ns, 1);
			info(0, "tunnel [%s]:%hu removed", lhost, lport);
			return controller_answer(cli, "tunnel [%s]:%hu removed",lhost,lport);
		}
	}

	return controller_answer(cli,"error: tunnel [%s]:%hu not found",lhost,lport);
}

/**
 * close a tunnel
 * @param[in] ns tunnel socket
 * @param[in] notify_server 0 if rdp2tcp server must be notified
 */
void tunnel_close(netsock_t *ns, int notify_server)
{
	unsigned char tid;
	//char host[NETADDRSTR_MAXSIZE];

	assert(valid_netsock(ns));

	tid = ns->tid;
	trace_tun("tid=0x%02x, notify=%i", tid, notify_server);

	if (tid != 0xff) {
		if (notify_server)
			channel_close_tunnel(tid);

		if (tid == last_tid)
			--last_tid;
	}

	netsock_cancel(ns);
}

/**
 * handle tcp-connect tunnel network accept-event
 * @param[in] srv tunnel socket
 */
void tunnel_accept_event(netsock_t *srv)
{
	unsigned char tid;
	netsock_t *cli;
	char host1[NETADDRSTR_MAXSIZE], host2[NETADDRSTR_MAXSIZE];

	assert(valid_netsock(srv) && (srv->type == NETSOCK_TUNSRV));
	trace_tun("");

	cli = netsock_accept(srv);
	if (cli) {
		cli->type = NETSOCK_TUNCLI;
		iobuf_init(&cli->u.tuncli.obuf, 'w', "tun");

		info(0, "accepted local tunnel client %s on %s",
				netaddr_print(&cli->addr, host1),
				netaddr_print(&srv->addr, host2));

		if (channel_is_connected()) {
			tid = channel_request_tunnel(srv->u.tunsrv.raf,
													srv->u.tunsrv.rhost,
													srv->u.tunsrv.rport, 0);

			if (tid != 0xff) {
				info(0, "reserved tunnel 0x%02x for %s",
						tid, netaddr_print(&cli->addr, host1));
				cli->tid = tid;
				cli->state = NETSTATE_CONNECTING;
			} else {
				netsock_close(cli);
			}

		} else {
			netsock_close(cli);
			error("channel not connected");
		}
	}
}

/**
 * handle remote tcp-connect/process tunnel network connect-event
 * @param[in] ns tunnel socket
 * @param[in] af remote address family (AF_INET/INET6/UNSPEC)
 * @param[in] addr remote tunnel address
 * @param[in] port remote tunnel TCP port
 */
void tunnel_connect_event(
			netsock_t *ns,
			int af,
			const void *addr,
			unsigned short port)
{
	unsigned int pid;
	char host[NETADDRSTR_MAXSIZE];

	assert(valid_netsock(ns) && (ns->type == NETSOCK_TUNCLI) && addr);

	trace_tun("id=0x%02x, af=%s, port=%hu",
		ns->tid,
		af == AF_INET ? "ipv4" : (af == AF_UNSPEC ? "proc" : "ipv6"), port);

	ns->state = NETSTATE_CONNECTED;

	if (af != AF_UNSPEC) {
		// tcp forwarding
		netaddr_set(af, addr, port, &ns->u.tuncli.raddr);
		info(0, "connected remote tunnel 0x%02x to %s",
			  ns->tid, netaddr_print(&ns->u.tuncli.raddr, host));
	} else {
		// process stdin/out forwarding
		pid = ntohl(*(unsigned int *)addr);
		ns->u.tuncli.raddr.pid = pid;
		ns->u.tuncli.is_process = 1;
		info(0, "connected remote tunnel 0x%02x to process %u",
			  ns->tid, ns->u.tuncli.raddr.pid);
	}
}

/**
 * handle tcp-listen tunnel network bind-event
 * @param[in] ns tunnel (NETSOCK_RTUNSRV)
 * @param[in] af remote address family (AF_INET/INET6/UNSPEC)
 * @param[in] addr remote tunnel address
 * @param[in] port remote tunnel TCP port
 */
void tunnel_bind_event(
			netsock_t *ns,
			int af,
			const void *addr,
			unsigned short port)
{
	assert(valid_netsock(ns) && (ns->type == NETSOCK_RTUNSRV) && addr && port);
	trace_tun("id=0x%02x, af=%s, port=%hu",
		ns->tid,
		af == AF_INET ? "ipv4" : "ipv6", port);

	ns->u.rtunsrv.bound = 1;
	netaddr_set(af, addr, port, &ns->addr);
}

/**
 * handle tcp-listen tunnel network connect-event
 * @param[in] srv tunnel (NETSOCK_RTUNSRV)
 * @param[in] new_id new tunnel id
 * @param[in] af remote address family (AF_INET/INET6)
 * @param[in] addr remote tunnel address
 * @param[in] port remote tunnel TCP port
 */
void tunnel_revconnect_event(
				netsock_t *srv,
				unsigned char new_id,
				int af,
				const void *addr,
				unsigned short port)
{
	netsock_t *cli;

	assert(valid_netsock(srv) && (srv->type == NETSOCK_RTUNSRV));
	trace_tun("new_id=0x%02x", new_id);
	
	cli = netsock_connect(srv->u.rtunsrv.lhost, srv->u.rtunsrv.lport);
	if (cli) {
		cli->type = NETSOCK_RTUNCLI;
		cli->tid = new_id;
		netaddr_set(af, addr, port, &cli->u.tuncli.raddr);
		iobuf_init(&cli->u.tuncli.obuf, 'w', "rtuncli");
	} else {
		channel_close_tunnel(new_id);
	}
}

/**
 * write data to tunnel client
 * @param[in] ns client socket
 * @param[in] buf data to write
 * @param[in] len size of buffer
 * @return -1 on error
 */
int tunnel_write(netsock_t *ns, const void *buf, unsigned int len)
{
	assert(valid_netsock(ns)
			&& ((ns->type == NETSOCK_TUNCLI) || (ns->type == NETSOCK_RTUNCLI)
				|| (ns->type == NETSOCK_S5CLI)));
	trace_tun("len=%u, state=%u", len, ns->state);

	return netsock_write(ns, buf, len);
}

/**
 * send tunnel queued data
 * @param[in] ns client socket
 * @return -1 on error
 */
int tunnel_write_event(netsock_t *ns)
{
	if ((ns->type == NETSOCK_RTUNCLI) && (ns->state != NETSTATE_CONNECTED))
		ns->state = NETSTATE_CONNECTED;

	return netsock_write(ns, NULL, 0);
}

/**
 * close all tunnels clients connections
 */
void tunnels_kill_clients(void)
{
	netsock_t *ns, *bak;
	char host[NETADDRSTR_MAXSIZE];

	list_for_each_safe(ns, bak, &all_sockets) {

		if (ns->type == NETSOCK_RTUNSRV) {
			ns->tid   = 0xff;
			ns->u.rtunsrv.bound = 0;
			memset(&ns->addr, 0, sizeof(ns->addr));

		} else if (ns->type > NETSOCK_CTRLCLI) {
			info(0, "closing tunnel client %s",
					netaddr_print(&ns->addr, host));
			netsock_close(ns);
		}
	}
}

/**
 * re-bind reverse-connect tunnels
 */
void tunnels_restart(void)
{
	netsock_t *ns, *bak;
	const char *rhost;
	unsigned short rport;
	
	list_for_each_safe(ns, bak, &all_sockets) {

		if (ns->type == NETSOCK_RTUNSRV) {

			rhost = &ns->u.rtunsrv.lhost[ns->u.rtunsrv.lhost_len];
			rport = ns->u.rtunsrv.rport;

			ns->tid = channel_request_tunnel(TUNAF_ANY, rhost, rport, 1);
			if (ns->tid != 0xff) {
				info(0, "restarted %s:%hu <-- %s:%hu",
						ns->u.rtunsrv.lhost, ns->u.rtunsrv.lport, rhost, rport);
			} else {
				error("failed to restart %s:%hu <-- %s:%hu",
						ns->u.rtunsrv.lhost, ns->u.rtunsrv.lport, rhost, rport);
				netsock_close(ns);
			}
		}
	}
}

