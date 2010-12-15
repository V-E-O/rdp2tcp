/**
 * @file main.c
 * main loop
 * @mainpage rdp2tcp
 * @section sec_ts TS virtual channel
 * @li channel.c
 * @section sec_tun rdp2tcp tunnels
 * @li tunnel.c
 * @li commands.c
 * @li socks5.c
 * @li controller.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * default rdp2tcp controller TCP port
 */
#define R2T_PORT   8477

extern struct list_head all_sockets;
static int killme = 0;

void bye(void)
{
	netsock_t *ns, *bak;

	list_for_each_safe(ns, bak, &all_sockets)
		netsock_close(ns);

	channel_kill();
	exit(0);
}

static void handle_cleanup(int sig)
{
	if (sig == SIGPIPE)
		info(0, "rdesktop pipe is broken");
	bye();
}

static void setup(int argc, char **argv)
{
	const char *host;
	int port;

	print_init();

	if (argc > 3)
		exit(0);

	if (argc == 3) {
		port = atoi(argv[2]);
		if ((port <= 0) || (port > 0xffff)) {
			error("invalid controller port %i", port);
			exit(0);
		}
		host = argv[1];
	} else if (argc == 2) {
		port = R2T_PORT;
		host = argv[1];
	} else {
		port = R2T_PORT;
		host = "127.0.0.1";
	}

	if (controller_start(host, port))
		exit(0);

	channel_init();
}

int main(int argc, char **argv)
{
	int ret, fd, max_fd, last_state, state;
	netsock_t *ns, *bak;
	fd_set rfd, wfd, *pwfd;
	struct timeval tv, *ptv;

	setup(argc, argv);

	signal(SIGUSR1, handle_cleanup);
	signal(SIGINT, handle_cleanup);
	signal(SIGPIPE, handle_cleanup);

	last_state = 0;

	while (!killme) {

		FD_ZERO(&rfd);
		FD_SET(RDP_FD_IN, &rfd);
		max_fd = RDP_FD_IN;

		FD_ZERO(&wfd);
		pwfd = NULL;
		ptv = NULL;
		state = channel_is_connected();
		if (state != last_state) {

			if (!state) // connected --> disconnected
				tunnels_kill_clients();
			else // disconnected --> connected
				tunnels_restart();
			
			last_state = state;
		}

		if (state) {
			// channel is connected
			if (channel_want_write()) {
				FD_SET(RDP_FD_OUT, &wfd);
				max_fd = RDP_FD_OUT;
				pwfd = &wfd;
			}
			tv.tv_sec  = 1;
			tv.tv_usec = 0;
			ptv = &tv;
		}

		list_for_each(ns, &all_sockets) {

			assert(valid_netsock(ns));

			if (ns->state != NETSTATE_CANCELLED) {
				fd = ns->fd;

				if (netsock_want_read(ns)) {
					FD_SET(fd, &rfd);
					if (fd > max_fd) max_fd = fd;
				}

				if (netsock_want_write(ns)) {
					FD_SET(fd, &wfd);
					pwfd = &wfd;
					if (fd > max_fd) max_fd = fd;
				}
			}
		}

		//debug(1, "channel connected: %i", channel_is_connected());

		ret = select(max_fd+1, &rfd, pwfd, NULL, ptv);
		if (ret == -1) {
			error("select error (%s)", strerror(errno));
			break;
		}
		
		if (ret == 0) {
			// channel ping timeout
			//info(0, "channel timeout");
			continue;
		}

		if (FD_ISSET(RDP_FD_OUT, &wfd))
			channel_write_event();

		if (FD_ISSET(RDP_FD_IN, &rfd)) {
			if (channel_read_event() < 0)
				break;
		}

		list_for_each_safe(ns, bak, &all_sockets) {

			assert(valid_netsock(ns));

			if (ns->state == NETSTATE_CANCELLED) {
				debug(0, "closing cancelled connection");
				netsock_close(ns);
				continue;
			}

			if (ns->type == NETSOCK_RTUNSRV)
				continue;

			fd = ns->fd;
			if (netsock_is_server(ns)) {
				// server socket
				if (FD_ISSET(fd, &rfd)) {
					if (ns->type == NETSOCK_TUNSRV)
						tunnel_accept_event(ns);
					else if (ns->type == NETSOCK_S5SRV)
						socks5_accept_event(ns);
					else
						controller_accept_event(ns);
				}

			} else {
				// client socket
				ret = 0;

				if (FD_ISSET(fd, &wfd))
					ret = tunnel_write_event(ns);

				if ((ret >= 0) && FD_ISSET(fd, &rfd)) {

					if (ns->type == NETSOCK_S5CLI)
						ret = socks5_read_event(ns);
					else if (ns->type == NETSOCK_CTRLCLI)
						ret = controller_read_event(ns);
					else
						ret = channel_forward_recv(ns);
				}

				if (ret < 0)
					netsock_close(ns);
			}
		}
	}

	bye();
	return 0;
}

