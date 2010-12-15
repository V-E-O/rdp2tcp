/**
 * @file channel.c
 * TS virtual channel management
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
#include "r2twin.h"
#include "rdp2tcp.h"
#include "msgparser.h"
#include "wtsapi32.h"

#ifndef CHANNEL_CHUNK_LENGTH
/** minimal chunk size supported by TS virtual channel */
#define CHANNEL_CHUNK_LENGTH 1600
#endif

#ifdef DEBUG
extern int debug_level;
#endif

static vchannel_t vc;

/**
 * check whether channel is connected
 */
int channel_is_connected(void)
{
	return vc.connected;
}

/**
 * initialize the TS virtual channel associated with rdp2tcp session
 * @param[in] name virtual channel name
 * @return 0 on success
 * @note only 1 virtual channel per server instance
 */
int channel_init(const char *name)
{
	HANDLE ts, *hbuf;
	DWORD buflen = 0;

	trace_chan("%s", name);
	memset(&vc, 0, sizeof(vc));

	ts = WTSVirtualChannelOpen(
				WTS_CURRENT_SERVER_HANDLE,
				WTS_CURRENT_SESSION,
				(LPSTR) name);
	if (!ts)
		return syserror("WTSVirtualChannelOpen");

	hbuf = NULL;
	buflen = sizeof(HANDLE *);
	if (!WTSVirtualChannelQuery(ts, WTSVirtualFileHandle, (void **)&hbuf, &buflen)) {
		syserror("WTSVirtualChannelQuery");
		WTSVirtualChannelClose(ts);
		return -1;
	}

	vc.ts = ts;
	vc.chan = *hbuf;
	WTSFreeMemory(hbuf);

	if (aio_init_forward(&vc.rio, &vc.wio, "chan")) {
		CloseHandle(vc.chan);
		WTSVirtualChannelClose(vc.ts);
		return -1;
	}

	events_init(vc.wio.io.hEvent, vc.rio.io.hEvent);

	return 0;
}

/**
 * destroy TS virtual channel associated with rdp2tcp session
 */
void channel_kill(void)
{
	trace_chan("");
	CancelIo(vc.chan);
	aio_kill_forward(&vc.rio, &vc.wio);
	CloseHandle(vc.chan);
	// TODO why does it throw invalid handle exception ?
	WTSVirtualChannelClose(vc.ts);
}

static int on_read_completed(iobuf_t *ibuf, void *bla)
{
	return commands_parse(ibuf);
}

/**
 * handle TS virtual channel read-event
 * @return 0 on success
 */
int channel_read_event(void)
{
	trace_chan("pending=%i", vc.rio.pending);
	return aio_read(&vc.rio, vc.chan, "chan", on_read_completed, NULL);
}

/**
 * check whether a async I/O write is pending
 */
int channel_write_pending(void)
{
	//trace_chan("pending=%i", (int)vc.wio.pending);
	return vc.wio.pending;
}

/**
 * process TS virtual channel write-event
 * @return 0 on success
 */
int channel_write_event(void)
{
	int ret;

	ret = aio_write(&vc.wio, vc.chan, "chan");
	trace_chan("pending=%i, outavail=%u, connected=%i, ret=%i",
			vc.wio.pending, iobuf_datalen(&vc.wio.buf), vc.connected, ret);


	if ((ret >= 0) ^ !!vc.connected) {
		info(0, "channel %sconnected", vc.connected?"dis":"");
		vc.connected ^= 1;
	}

	return 0;
}

/**
 * send a message through TS virtual channel
 * @param[in] cmd rdp2tcp command (R2TCMD_xxx)
 * @param[in] tun_id rdp2tcp tunnel ID
 * @param[in] data data to write
 * @param[in] data_len size of buffer
 * @return 0 on success
 */
int channel_write(
	unsigned char cmd,
	unsigned char tun_id,
	const void *data,
	unsigned int data_len)
{
	unsigned char *ptr;
	unsigned int used;

	trace_chan("cmd=%02x id=%02x len=%u", cmd, tun_id, data_len);
	used = iobuf_datalen(&vc.wio.buf);

	ptr = iobuf_reserve(&vc.wio.buf, data_len+6, NULL);
	if (!ptr)
		return error("failed to append %u bytes to channel buffer", data_len+6);
	*((unsigned int *)ptr) = htonl(data_len+2);

	ptr[4] = cmd;
	ptr[5] = tun_id;
	memcpy(ptr+6, data, data_len);
	iobuf_commit(&vc.wio.buf, data_len+6);

	if (used > 0)
		return 0;

	return channel_write_event();
}

/**
 * forward tunnel input buffer to virtual channel
 * @param[in] tun tunnel
 * @return -1 on error
 */
int channel_forward(tunnel_t *tun)
{
	iobuf_t *ibuf;
	unsigned int len;
	int ret;

	ibuf = &tun->rio.buf;
	len = iobuf_datalen(ibuf);
	ret = 0;

	if (len > 0) {
		ret = channel_write(R2TCMD_DATA, tun->id, iobuf_dataptr(ibuf), len);
		if (ret >= 0)
			iobuf_consume(ibuf, len);
	}

	return ret;
}

