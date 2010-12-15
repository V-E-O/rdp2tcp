/**
 * @file events.c
 * async loop helpers
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
#include "r2twin.h"
#include "rdp2tcp.h"

extern struct list_head all_tunnels;

static unsigned int events_count = 0;
static HANDLE all_events[0x102] = {0, };
static unsigned char evtid_to_tunid[0x102] = {0, };

/** initialize the TS events loop
 * @param[in] wevt TS virtual channel write-event
 * @param[in] revt TS virtual channel read-event */
void events_init(HANDLE wevt, HANDLE revt)
{
	trace_evt("wevt=%x, revt=%x", wevt, revt);
	all_events[0] = wevt;
	all_events[1] = revt;
	events_count = 2;
}

/** register a network tunnel event
 * @param[in] evt TS virtual channel socket event
 * @param[in] id rdp2tcp tunnel ID
 * @return 0 on success */
int event_add_tunnel(HANDLE evt, unsigned char id)
{
	unsigned int i;

	trace_evt("evt=%x, id=0x%02x", evt, id);

	i = events_count;
	if (i >= 0x101)
		return -1;

	all_events[i] = evt;
	evtid_to_tunid[i] = id;

	++events_count;

	return 0;
}

/** register a process tunnel event
 * @param[in] proc child process handle
 * @param[in] re child process read event
 * @param[in] we child process write event
 * @param[in] id rdp2tcp tunnel ID
 * @return 0 on success */
int event_add_process(HANDLE proc, HANDLE re, HANDLE we, unsigned char id)
{
	unsigned int i;

	trace_evt("proc=%x, revt=%x, wevt=%x, id=%u", proc, re, we, id);

	i = events_count;
	if (i+2 >= 0x101)
		return -1;

	all_events[i] = proc;
	all_events[i+1] = re; // read overlapped event
	all_events[i+2] = we; // write overlapped event

	evtid_to_tunid[i] = id;
	evtid_to_tunid[i+1] = id;
	evtid_to_tunid[i+2] = id;

	events_count += 3;

	return 0;
}

/** remove events associated with a rdp2tcp tunnel
 * @param[in] id rdp2tcp tunnel ID */
void event_del_tunnel(unsigned char id)
{
	unsigned int i, j;

	trace_evt("id=0x%02x", id);

	for (i=2, j=0; i<events_count; ++i) {
		if (evtid_to_tunid[i] == id)
			++j;
		else if (j)
			break;
	}

	if (j > 0) {
		if (i < events_count) {
			memmove(&evtid_to_tunid[i-j], &evtid_to_tunid[i],
						sizeof(evtid_to_tunid[0]) * (events_count-i));
			memmove(&all_events[i-j], &all_events[i],
						sizeof(all_events[0]) * (events_count-i));
		}
		events_count -= j;
	}
}

/** wait for tunnel events
 * @param[out] out_tun tunnel associated with last event
 * @param[out] out_h last event handle
 * @return the last event type (EVT_xxx) or -1 on error */
int event_wait(tunnel_t **out_tun, HANDLE *out_h)
{
	DWORD ret, off;
	tunnel_t *tun;

	off = (channel_write_pending() ? 0 : 1);

	ret = WaitForMultipleObjects(events_count-off, &all_events[off], FALSE,
											RDP2TCP_PING_DELAY*1000);
	
	if (ret == WAIT_FAILED) {
		assert(GetLastError() != ERROR_INVALID_HANDLE);
		return syserror("WaitForMultipleObjects");
	}

	if (ret == WAIT_TIMEOUT)
		return EVT_PING;

	ret -= WAIT_OBJECT_0;
	trace_evt("off=%i --> 0x%x (evt=0x%x)", off, ret, all_events[off+ret]);

	if (ret == 0)
		return (off == 0 ? EVT_CHAN_WRITE : EVT_CHAN_READ);
		
	if ((ret == 1) && (off == 0))
		return EVT_CHAN_READ;

		tun = tunnel_lookup(evtid_to_tunid[off+ret]);
		if (!tun)
			return error("invalid tunnel event 0x%02x", evtid_to_tunid[off+ret]);

		*out_tun = tun;
		*out_h   = all_events[off+ret];
		return EVT_TUNNEL;
}


