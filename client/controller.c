/**
 * @file controller.c
 * rdp2tcp controller
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

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#ifndef PTR_DIFF
#define PTR_DIFF(e,s) \
	        ((unsigned int)(((unsigned long)(e))-((unsigned long)(s))))
#endif

/**
 * send an answer to the controller client
 * @param[in] cli controller client socket
 * @param[in] fmt format string
 * @return -1 on error
 */
int controller_answer(netsock_t *cli, const char *fmt, ...)
{
	int ret;
	va_list va;
	char buf[256];

	assert(valid_netsock(cli) && fmt && *fmt);

	va_start(va, fmt);
	ret = vsnprintf(buf, sizeof(buf)-2, fmt, va);
	va_end(va);

	if (ret > 0) {
		buf[ret] = '\n';
		ret = netsock_write(cli, buf, ret+1);
	} else {
		ret = error("failed to prepare controller answer");
	}

	return ret;
}


/**
 * start controller server
 * @param[in] host local hostname
 * @param[in] port local tcp port
 * @return 0 on success
 */
int controller_start(const char *host, unsigned short port)
{
	netsock_t *ns;

	assert(host && *host && port);
	trace_ctrl("host=%s, port=%hu", host, port);

	ns = netsock_bind(NULL, host, port, 0);
	if (!ns)
		return -1;

	ns->type  = NETSOCK_CTRLSRV;
	info(0, "controller listening on %s:%hu", host, port);

	return 0;
}

/**
 * handle controller network accept-event
 * @param[in] ns controller socket
 */
void controller_accept_event(netsock_t *ns)
{
	netsock_t *cli;
	char buf[NETADDRSTR_MAXSIZE];

	assert(valid_netsock(ns) && (ns->type == NETSOCK_CTRLSRV));
	trace_ctrl("");

	cli = netsock_accept(ns);
	if (cli) {
		cli->type = NETSOCK_CTRLCLI;
		cli->tid  = 0xff;
		iobuf_init2(&cli->u.ctrlcli.ibuf, &cli->u.ctrlcli.obuf, "ctrl");
		info(1, "accepted controller %s", netaddr_print(&cli->addr, buf));
	}
}

extern struct list_head all_sockets;

static int dump_sockets(netsock_t *cli)
{
	int ret;
	netsock_t *ns;
	char host1[NETADDRSTR_MAXSIZE], host2[NETADDRSTR_MAXSIZE];

	assert(valid_netsock(cli));

	ret = 0;

	list_for_each(ns, &all_sockets) {

		if (ns == cli)
			continue;

		if (ns->addr.ip4.sin_family)
			netaddr_print(&ns->addr, host1);

		switch (ns->type) {

			case NETSOCK_CTRLSRV:
				ret = controller_answer(cli, "ctrlsrv %s", host1);
				break;

			case NETSOCK_TUNSRV:
				if (!ns->u.tunsrv.rport) {
					ret = controller_answer(cli, "tunsrv  %s %s", host1,
							ns->u.tunsrv.rhost);
				} else {
					ret = controller_answer(cli, "tunsrv  %s %s:%hu", host1,
							ns->u.tunsrv.rhost, ns->u.tunsrv.rport);
				}
				break;

			case NETSOCK_S5SRV:
				ret = controller_answer(cli, "s5srv   %s", host1);
				break;

			case NETSOCK_CTRLCLI:
				ret = controller_answer(cli, "ctrlcli %s", host1);
				break;

			case NETSOCK_TUNCLI:
				if (!ns->state != NETSTATE_CONNECTED) {
					ret = controller_answer(cli, "tuncli  %s tid=%hu",
						host1, ns->tid);
					break;
				}


				if (ns->u.tuncli.is_process) {
					ret = controller_answer(cli, "tuncli  %s 0x%x pid %u",
									host1, ns->tid,
									*(unsigned int *)&ns->u.tuncli.raddr.pid);
					break;
				}

				ret = controller_answer(cli, "tuncli  %s 0x%x %s",
									host1, ns->tid,
									netaddr_print(&ns->u.tuncli.raddr, host2));
				break;

			case NETSOCK_S5CLI:
				ret = controller_answer(cli, "s5cli   %s 0x%x",
											host1, ns->tid);
				break;

			case NETSOCK_RTUNSRV:
				ret = controller_answer(cli, "rtunsrv %s:%hu %s:%hu 0x%x",
										ns->u.rtunsrv.lhost, ns->u.rtunsrv.lport,
										&ns->u.rtunsrv.lhost[ns->u.rtunsrv.lhost_len],
										ns->u.rtunsrv.rport, ns->tid);
				break;

			//case NETSOCK_RTUNCLI:
			default:
				ret = controller_answer(cli, "rtuncli %s 0x%x %s",
											host1, ns->tid,
											netaddr_print(&ns->u.tuncli.raddr, host2));
				break;
		}

		if (ret)
			break;
	}

	if (ret >= 0)
		ret = controller_answer(cli, "\n");

	return ret;
}

static char *extract_port(char *data, unsigned short *out_port)
{
	char *ptr, *end;
	long port;
	
	ptr = strchr(data, ' ');
	if (!ptr)
		return NULL;
	*ptr = 0;

	end = NULL;
	port = strtol(ptr+1, &end, 10);
	if (!end || (port <= 0) || (port > 0xffff))
		return NULL;

	if (*end && (*end != ' '))
		return NULL;

	*out_port = (unsigned short) port;

	return end;
}

/**
 * handle controller network read-event
 * @param[in] cli controller socket
 */
int controller_read_event(netsock_t *cli)
{
	char cmd, *data, *end, *lhost, *rhost;
	int ret;
	unsigned int avail, parsed;
	unsigned short lport, rport;
	const char valid_commands[] = "ltrxs-";
	char host[NETADDRSTR_MAXSIZE];

	assert(valid_netsock(cli) && (cli->type == NETSOCK_CTRLCLI));
	trace_ctrl("");

	ret = netsock_read(cli, &cli->u.ctrlcli.ibuf, 0, NULL);
	if (ret)
		return ret;

	data   = iobuf_dataptr(&cli->u.ctrlcli.ibuf);
	avail  = iobuf_datalen(&cli->u.ctrlcli.ibuf);
	assert(avail);
	parsed = 0;

	// for each line
	do {

		end = memchr(data, '\n', avail-parsed);
		if (!end) {
			ret = 1;
			break;
		}
		*end = 0;
		if (!*data) goto badproto;

		parsed += PTR_DIFF(end, data) + 1;

		if (end[-1] == '\r')
			end[-1] = 0;

		cmd = *data;
		if (!strchr(valid_commands, cmd))
			goto badproto;

		debug(0, "cmd=\"%s\"", data);

		if (cmd == 'l') { // list sockets
			ret = dump_sockets(cli);

		} else {
			// commands with argc >= 2

			if (*++data != ' ') goto badproto;
			if (!*++data) goto badproto;

			lhost = data;
			data = extract_port(data, &lport);
			if (!data) goto badproto;

			if (cmd == '-') { // remove tunnel
				ret = tunnel_del(cli, lhost, lport);

			} else if (cmd == 's') { // add socks5 server
				ret = socks5_bind(cli, lhost, lport);

			} else {
				// commands with argc >= 3

				if (*data++ != ' ') goto badproto;
				if (!*data) goto badproto;

				if (cmd == 'x') { // exec & forward stdin/stdout
					ret = tunnel_add(cli, lhost, lport, AF_UNSPEC, data, 0);

				} else {
					// commands with argc == 4
					
					rhost = data;
					if (!extract_port(data, &rport))
						return -1;
					
					if (cmd == 't') { // add TCP tunnel
						ret = tunnel_add(cli, lhost, lport,
												AF_UNSPEC, rhost, rport);

					} else { // cmd == 'r' reverse TCP connect
						ret = tunnel_add_reverse(cli, lhost, lport,
														AF_UNSPEC, rhost, rport);
					}
				}
			}
		}

		data = end + 1;

	} while (!ret && (parsed < avail));

	if (parsed > 0)
		iobuf_consume(&cli->u.ctrlcli.ibuf, parsed);

	return ret;

badproto:
	info(0, "closing controller %s (bad protocol)",
			netaddr_print(&cli->addr,host));
	return -1;
}

