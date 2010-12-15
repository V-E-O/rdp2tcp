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
#ifndef __R2TWIN_H__
#define __R2TWIN_H__

#include "compiler.h"
#include "debug.h"
#include "print.h"
#include "list.h"
#include "iobuf.h"
#include "nethelper.h"

/** async I/O instance */
typedef struct _aio {
	iobuf_t buf;   /**< I/O buffer */
	unsigned int min_io_size; /**< minimal I/O buffer size */
	int pending;   /**< 1 if an I/O is pending */
	OVERLAPPED io; /**< async event */
} aio_t;

/** TS virtual channel */
typedef struct _vchannel {
	HANDLE ts;       /**< RDP channel handle */
	HANDLE chan;     /**< RDP channel I/O handle */
	int connected:1; /**< 1 if channel is conneced */
	aio_t rio;       /**< input aio_t */
	aio_t wio;       /**< output aio_t */
} vchannel_t;

/** rdp2tcp tunnel */
typedef struct _tunnel {
	struct list_head list;   /**< double-linked list */
	sock_t sock;             /**< tunnel socket */
	unsigned char connected; /**< 1 if tunnel is connected */
	unsigned char server;    /**< 1 for reverse-connect tunnel */
	unsigned char id;        /**< tunnel identifier */
	HANDLE proc;     /**< child process HANDLE */
	HANDLE rfd;      /**< child process stdout/stderr HANDLE */
	HANDLE wfd;      /**< child process stdin HANDLE */
	aio_t rio;       /**< input aio_t */
	aio_t wio;       /**< output aio_t */
	netaddr_t addr;  /**< network address */
} tunnel_t;

/* aio.c ***/
#define valid_aio(aio) ((aio) && valid_iobuf(&(aio)->buf) && (aio)->io.hEvent)
#ifdef DEBUG
int __aio_init_forward(aio_t *, aio_t *, const char *);
#define aio_init_forward(rio, wio, name) __aio_init_forward(rio, wio, name)
#else
int __aio_init_forward(aio_t *, aio_t *);
#define aio_init_forward(rio, wio, name) __aio_init_forward(rio, wio)
#endif

void aio_kill_forward(aio_t *, aio_t *);

typedef int (*aio_readcb_t)(iobuf_t *, void *);
int aio_read(aio_t *, HANDLE, const char *, aio_readcb_t, void *);
int aio_write(aio_t *, HANDLE, const char *);

/* events ***/
#define EVT_CHAN_WRITE 0
#define EVT_CHAN_READ  1
#define EVT_TUNNEL     2
#define EVT_PING       3

void events_init(HANDLE, HANDLE);
int event_add_tunnel(HANDLE, unsigned char);
void event_del_tunnel(unsigned char);
int event_add_process(HANDLE, HANDLE, HANDLE, unsigned char);
int event_wait(tunnel_t **, HANDLE *);

/* channel.c ***/
int channel_init(const char *);
void channel_kill(void);
int channel_is_connected(void);
int channel_read_event(void);
int channel_write_event(void);
int channel_write_pending(void);
int channel_write(unsigned char, unsigned char, const void *, unsigned int);
int channel_forward(tunnel_t *);

/* tunnel.c ***/
#define valid_tunnel(tun) ((tun) && (tun)->list.next && (tun)->list.prev)
void tunnel_create(unsigned char, int, const char *, unsigned short, int);
tunnel_t *tunnel_lookup(unsigned char);
int tunnel_event(tunnel_t *, HANDLE);
int tunnel_write(tunnel_t *tun, const void *, unsigned int);
void tunnel_close(tunnel_t *);
void tunnels_kill(void);

/* errors.c ***/
int wsaerror(const char *);
int syserror(const char *);

/* process.c ***/
int  process_start(tunnel_t *, const char *);
void process_stop(tunnel_t *);

/* main.c ***/
void bye(void);

#endif
