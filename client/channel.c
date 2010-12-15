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
#include "r2tcli.h"
#include "msgparser.h"

#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

extern int debug_level;

/** TS virtual channel singleton  */
typedef struct _vchannel {
	time_t ts;      /**< timestamp of last channel activity */
	int last_state; /**< virtual channel previous state */
	iobuf_t ibuf;   /**< input buffer */
	iobuf_t obuf;   /**< output buffer */
} vchannel_t;

static vchannel_t vc;

/**
 * initialize TS virtual channel
 */
int channel_init(void)
{
	trace_chan("");

	vc.ts = 0;
	vc.last_state = -1;
	iobuf_init2(&vc.ibuf, &vc.obuf, "chan");

	return 0;
}

/**
 * destroy TS virtual channel I/O buffers
 */
void channel_kill(void)
{
	trace_chan("");

	iobuf_kill2(&vc.ibuf, &vc.obuf);
}

/**
 * check whether virtual channel is currently connected
 * @return 0 if rcp2tcp.exe is not started on TS server
 */
int channel_is_connected(void)
{
	int connected;
	time_t now;

	time(&now);

	connected = (vc.ts && (vc.ts + RDP2TCP_PING_DELAY + 4 > now));
	//trace_chan(connected ? "yes" : "no");

	if (vc.last_state != connected) {
		vc.last_state = connected;
		info(0, "virtual channel %s", connected?"connected":"disconnected");
	}

	return connected;
}

/**
 * handle virtual channel read-event
 * @return 0 on success
 */
int channel_read_event(void)
{
	ssize_t r;
	char *ptr;
	unsigned int msglen, avail;
	
	//trace_chan("");

	ptr = (char *)&msglen;
	avail = 4;
	do {
		r = read(RDP_FD_IN, ptr, avail);
		if (r <= 0)
			goto chan_read_err;
		ptr += r;
		avail -= r;
	} while (avail > 0);

	ptr = iobuf_reserve(&vc.ibuf, msglen, &avail);
	if (!ptr)
		return error("failed to reserve channel memory");

	do {
		r = read(RDP_FD_IN, ptr, msglen);
		//trace_chan("r=%u/%u", r, msglen);
		if (r < 0)
			goto chan_read_err;

#ifdef DEBUG
		if (debug_level > 2) {
			fputs("[in] ", stderr);
			fprint_hex(ptr, r, stderr);
			fputc('\n', stderr);
		}
#endif
		
		print_xfer("chan", 'r', (unsigned int)r);

		ptr += r;
		msglen -= r;
	} while (msglen > 0);

	iobuf_commit(&vc.ibuf, r);
	commands_parse(&vc.ibuf);
	time(&vc.ts);

	return 0;

chan_read_err:
	if (r < 0)
		error("failed to read from channel pipe (%s)", strerror(errno));
	else if (r == 0)
		error("channel closed");
	return -1;	
}

/**
 * check whether data must be written to the TS virtual channel
 * @return 0 if virtual channel output buffer is empty
 */
int channel_want_write(void)
{
	//trace_chan(iobuf_datalen(&vc.obuf) > 0 ? "yes" : "no");
	return iobuf_datalen(&vc.obuf) > 0;
}

/**
 * handle virtual channel write-event
 * @return 0 on success
 */
void channel_write_event(void)
{
	int ret, fd;
	unsigned int w;

	trace_chan("");
#ifdef DEBUG
	if (debug_level > 2) iobuf_dump(&vc.obuf);
#endif

	fd = RDP_FD_OUT;
	ret = net_write(&fd, &vc.obuf, NULL, 0, &w);
	if (ret >= 0) {
		if (w > 0)
			print_xfer("chan", 'w', (unsigned int) w);

	} else { 
		if (ret == NETERR_CLOSED) 
			error("rdesktop pipe closed");
		else
			error("failed to write to rdesktop pipe (%s)", strerror(errno));
		bye();
	}
}

/**
 * reserve memory into virtual channel ouput buffer
 * @param[in] size requested minimal buffer size
 * @param[out] out_avail allocated size
 * @return NULL on memory allocation error
 */
static void *write_reserve(unsigned int size, unsigned int *out_avail)
{
	char *ptr;
	unsigned int avail;

	assert(size || out_avail);
	//trace_chan("");

	// need extra space for size header
	ptr = iobuf_reserve(&vc.obuf, size+4, &avail);
	if (!ptr) {
		error("failed to allocate channel memory");
		return NULL;
	}

	if (out_avail)
		*out_avail = avail - 4;

	return ptr + 4;
}

/**
 * commit memory into virtual channel output buffer
 * @param[in] size commited buffer size
 */
static void write_commit(unsigned int size)
{
	assert(size);
	//trace_chan("size=%u", size);

	*(unsigned int *)(iobuf_allocptr(&vc.obuf)) = htonl(size);
	iobuf_commit(&vc.obuf, size+4);
}

/**
 * send a ping message to rdp2tcp server
 * @return 0 if message cannot be queued
 */
int channel_ping(void)
{
	r2tmsg_t msg;

	trace_chan("");
	msg.cmd = R2TCMD_PING;
	msg.id  = 0;

	return !iobuf_append(&vc.obuf, &msg, 2);
}

/**
 * function called whenever a ping message is sent by rdp2tcp server
 */
void channel_pong(void)
{
	//trace_chan("");

	if (vc.last_state != 1) {
		vc.last_state = 1;
		info(0, "virtual channel connected");
	}
	time(&vc.ts);
}

#if 0
// purify/valgrind/amd64
static unsigned int purify_strlen(const char *x)
{
	unsigned int i;
	i = 0;
	while (*x++) ++i;
	return i;
}
#endif

/**
 * send a rdp2tcp tunnel request command to the rdp2tcp server
 * @param[in] tunaf preferred address family (TUNAF_IPV4/IPV6/ANY)
 * @param[in] rhost remote tunnel hostname
 * @param[in] rport remote tunnel port
 * @param[in] reverse_connect 0 for tcp-connect or 1 for tcp-bind
 * @return the tunnel ID or 0xff on error
 */
unsigned char channel_request_tunnel(
							unsigned char tunaf,
							const char *rhost,
							unsigned short rport,
							int reverse_connect)
{
	unsigned char tid;
	unsigned int hlen;
	r2tmsg_connreq_t *msg;

	assert((tunaf <= TUNAF_IPV6) && rhost && *rhost);
	trace_chan("tunaf=0x%02x, rhost=%s, rport=%hu", tunaf, rhost, rport);

	tid = tunnel_generate_id();
	if (tid == 0xff)
		return 0xff;

	hlen = 1 + strlen(rhost);
	msg = write_reserve(5 + hlen, NULL);
	if (!msg)
		return 0xff;

	msg->cmd  = (!reverse_connect ? R2TCMD_CONN : R2TCMD_BIND);
	msg->id   = tid;
	msg->port = htons(rport);
	msg->af   = tunaf;
	memcpy(msg->hostname, rhost, hlen);

	write_commit(5 + hlen);

	return tid;
}

/**
 * notify the server a tunnel has been closed
 * @param[in] tid the tunnel ID
 */
void channel_close_tunnel(unsigned char tid)
{
	r2tmsg_t *msg;

	assert(tid != 0xff);
	trace_chan("tid=0x%02x", tid);

	msg = write_reserve(2, NULL);
	if (msg) {
		msg->cmd = R2TCMD_CLOSE;
		msg->id  = tid;
		write_commit(2);
	}
}

/**
 * receive data from tcp tunnel and forward it to the RDP channel
 * @param[in] ns tunnel socket
 * @return 0 or 1 on success
 */
int channel_forward_recv(netsock_t *ns)
{
	int ret;
	unsigned int r, off;
	unsigned char *msg;

	assert(valid_netsock(ns) && ((ns->type == NETSOCK_TUNCLI)
			|| (ns->type == NETSOCK_RTUNCLI) || (ns->type == NETSOCK_S5CLI)));
	trace_chan("id=0x%02x", ns->tid);

	off = iobuf_datalen(&vc.obuf);
	ret = netsock_read(ns, &vc.obuf, 6, &r);
	if (!ret) {
		msg = iobuf_dataptr(&vc.obuf) + off;
		*(unsigned int*)msg = htonl(r + 2);
		msg[4] = R2TCMD_DATA;
		msg[5] = ns->tid;
	}

	if (ret < 0)
		tunnel_close(ns, 1);

	return 0;
}

/**
 * forward data from I/O buffer to the RDP channel
 * @param[in] ibuf input buffer
 * @param[in] tid tunnel identifier
 * @return 0 or 1 on success
 */
int channel_forward_iobuf(iobuf_t *ibuf, unsigned char tid)
{
	r2tmsg_t *msg;
	unsigned int len;

	assert(valid_iobuf(ibuf) && (tid != 0xff));
	trace_chan("tid=0x%02x", tid);

	len = iobuf_datalen(ibuf);
	assert(len > 0);

	msg = write_reserve(len+2, NULL);
	if (!msg)
		return -1;

	msg->cmd = R2TCMD_DATA;
	msg->id  = tid;
	memcpy(((char *)msg)+2, iobuf_dataptr(ibuf), len);
	write_commit(len + 2);

	iobuf_consume(ibuf, len);

	return 0;
}

