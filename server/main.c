/**
 * @file main.c
 * main loop
 * @mainpage main server loop
 * @section sec_ts virtual channel
 * @li channel.c
 * @section sec_tun rdp2tcp tunnels
 * @li tunnel.c
 * @li process.c
 * @section sec_aio async helpers
 * @li events.c
 * @li aio.c
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
#include "print.h"
#include "rdp2tcp.h"
#include "r2twin.h"

#include <stdio.h>
#include <time.h>

void bye(void)
{
	channel_kill();
	tunnels_kill();
	net_exit();
	exit(0);
}

static BOOL WINAPI on_signal(DWORD sig)
{
	switch (sig) {
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
			bye();
			return TRUE;
	}

	return FALSE;
}

static void setup(void)
{
	print_init();
	net_init();
	SetConsoleCtrlHandler(on_signal, TRUE);
}

static void usage(char *n)
{
	fprintf(stderr, "usage: %s [vname]\n", n);
	exit(0);
}

static time_t last_ping = 0;

static int ping(time_t *now)
{

	time(now);
	if (!last_ping || (last_ping + RDP2TCP_PING_DELAY - 1 < *now)) {
		last_ping = *now;
		return channel_write(R2TCMD_PING, 0, NULL, 0);
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret;
	const char *chan_name;
	tunnel_t *tun;
	HANDLE h;
	time_t now;

	if (argc > 2)
		usage(argv[0]);

	chan_name = (argc == 2 ? argv[1] : RDP2TCP_CHAN_NAME);

	setup();

	do {
		if (channel_init(chan_name))
			break;

		ret = ping(&now);

		// I/O loop
		while (ret >= 0) {

			switch (event_wait(&tun, &h)) {

				case EVT_CHAN_WRITE: // virtual channel outgoing data
					debug(0, "EVT_CHAN_WRITE");
					ret = channel_write_event();
					if (!ret)
						last_ping = now;
					break;

				case EVT_CHAN_READ: // virtual channel incoming data
					debug(0, "EVT_CHAN_READ");
					ret = channel_read_event();
					if (ret >= 0)
						ping(&now);
					break;

				case EVT_TUNNEL: // tcp tunnel incoming/outgoing data
					debug(0, "EVT_TUNNEL");
					ret = tunnel_event(tun, h);
					break;

				case EVT_PING: // ping delay
					if (channel_is_connected()) {
						debug(0, "EVT_PING");
						ret = ping(&now);
					} else {
						debug(0, "channel still disconnected");
					}
					break;

				default:
					ret = -1;

			}

		}

		channel_kill();
		Sleep(1000);

	} while (1);

	bye();
	return 0;
}

