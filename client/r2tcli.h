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
#ifndef __R2TCLIENT_H__
#define __R2TCLIENT_H__

#include "debug.h"
#include "print.h"
#include "list.h"
#include "iobuf.h"
#include "rdp2tcp.h"
#include "nethelper.h"

#include <sys/types.h>
#include <sys/socket.h>

// netsock.c
#define NETSOCK_CTRLSRV 0
#define NETSOCK_TUNSRV  1
#define NETSOCK_S5SRV   2
#define NETSOCK_CTRLCLI 3
#define NETSOCK_TUNCLI  4
#define NETSOCK_S5CLI   5
#define NETSOCK_RTUNSRV 6
#define NETSOCK_RTUNCLI 7
#define NETSOCK_UNDEF   0xff

#define NETSTATE_INIT           0
#define NETSTATE_CANCELLED      1
#define NETSTATE_CONNECTING     2
#define NETSTATE_CONNECTED      3
#define NETSTATE_AUTHENTICATING 4
#define NETSTATE_AUTHENTICATED  5

/** network socket (tunnel, client or server) */
typedef struct _netsock {
	struct list_head list;     /**< double-linked list */
	int fd;                    /**< socket descriptor */
	unsigned char type;        /**< socket type */
	unsigned char state;       /**< tunnel state */
	unsigned char tid;         /**< tunnel identifier */
	unsigned int min_io_size;  /**< minimal input buffer size */
	netaddr_t addr;            /**< socket address */
	union {
		struct {
			unsigned char  raf;   /**< remote address family */
			unsigned short rport; /**< remote port */
			char rhost[0];        /**< remote host */
		} tunsrv;
		struct {
			iobuf_t obuf;             /**< output buffer */
			netaddr_t raddr;          /**< remote address */
			unsigned char is_process; /**< 1 if tunnel is a process */
		} tuncli;
		struct {
			iobuf_t obuf; /**< output buffer */
			iobuf_t ibuf; /**< input buffer */
		} ctrlcli;
		struct {
			iobuf_t obuf; /**< output buffer */
			iobuf_t ibuf; /**< input buffer */
		} sockscli;
		struct {
			unsigned short lport;     /**< local port */
			unsigned short rport;     /**< remote port */
			unsigned short lhost_len; /**< size of local host string */
			unsigned char bound;      /**< 1 if remote server is listening */
			char lhost[0];            /**< local host followed by remote host */
		} rtunsrv;
	} u;
} netsock_t;

#define valid_netsock(ns) \
				((ns) && (ns)->list.next && (ns)->list.prev \
				 && (((ns)->fd != -1) || ((ns)->type == NETSOCK_RTUNSRV)) \
				 && ((ns)->type <= NETSOCK_RTUNCLI) \
				 && (((ns)->addr.ip4.sin_family == AF_INET) \
					 || ((ns)->addr.ip4.sin_family == AF_INET6) \
					 || ((ns)->type == NETSOCK_RTUNSRV)))

#define netsock_is_server(ns) ((ns)->type <= NETSOCK_S5SRV)

/**
 * check if main loop must wait for network-read event
 * @param[in] ns netsock socket
 */
#define netsock_want_read(ns) ((ns)->state >= NETSTATE_CONNECTED)

netsock_t *netsock_alloc(netsock_t *, int, netaddr_t *, unsigned int);
netsock_t *netsock_bind(netsock_t *, const char*,unsigned short,unsigned int);
netsock_t *netsock_accept(netsock_t *);
netsock_t *netsock_connect(const char *, unsigned short);
int netsock_read(netsock_t *, iobuf_t *, unsigned int, unsigned int *);
int  netsock_write(netsock_t *, const void *, unsigned int);
int  netsock_want_write(netsock_t *);
void netsock_cancel(netsock_t *);
void netsock_close(netsock_t *);

// channel.c
#define RDP_FD_IN  0
#define RDP_FD_OUT 1

int  channel_init(void);
void channel_kill(void);
int  channel_is_connected(void);
int  channel_read_event(void);
int  channel_want_write(void);
void channel_write_event(void);
int  channel_ping(void);
void channel_pong(void);
unsigned char channel_request_tunnel(unsigned char, const char *, unsigned short, int);
int channel_forward_recv(netsock_t *);
int channel_forward_iobuf(iobuf_t *, unsigned char);
void channel_close_tunnel(unsigned char);

// controller.c
int  controller_start(const char *, unsigned short);
void controller_accept_event(netsock_t *);
int  controller_read_event(netsock_t *);
int  controller_answer(netsock_t *, const char *, ...);

// tunnel.c
int tunnel_add(netsock_t *, char *, unsigned short, int, char *, unsigned short);
int tunnel_add_reverse(netsock_t *, char *, unsigned short, int, char *, unsigned short);
int tunnel_del(netsock_t *, char *, unsigned short);
void tunnel_accept_event(netsock_t *);
void tunnel_connect_event(netsock_t *, int, const void *, unsigned short);
void tunnel_revconnect_event(netsock_t *, unsigned char, int,
										const void *, unsigned short);
void tunnel_bind_event(netsock_t *, int, const void *, unsigned short);
int  tunnel_write_event(netsock_t *);
int  tunnel_write(netsock_t *, const void *, unsigned int);
void tunnel_close(netsock_t *, int);
unsigned char tunnel_generate_id(void);
netsock_t *tunnel_lookup(unsigned char);
void tunnels_kill_clients(void);
void tunnels_restart(void);

// socks5.c
int socks5_bind(netsock_t *, const char *, unsigned short);
void socks5_connect_event(netsock_t *, int, const void *, unsigned short);
void socks5_accept_event(netsock_t *);
int  socks5_read_event(netsock_t *);

// main.c
void bye(void);

#endif
