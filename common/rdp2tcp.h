/**
 * @file rdp2tcp.h
 * rdp2tcp protocol specification
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
#ifndef __RDP2TCP_H__
#define __RDP2TCP_H__

#include "compiler.h"

#define RDP2TCP_MAX_MSGLEN (512*1024)
/**
 *  default TS virtual channel name
 */
#define RDP2TCP_CHAN_NAME "rdp2tcp"
#define RDP2TCP_PING_DELAY 5 // secs

// rdp2tcp commands
#define R2TCMD_CONN  0x00
#define R2TCMD_CLOSE 0x01
#define R2TCMD_DATA  0x02
#define R2TCMD_PING  0x03
#define R2TCMD_BIND  0x04
#define R2TCMD_RCONN 0x05
#define R2TCMD_MAX   0x06

// address family on wire
#define TUNAF_ANY  0x00
#define TUNAF_IPV4 0x01
#define TUNAF_IPV6 0x02

// rdp2tcp error codes
#define R2TERR_SUCCESS     0x00
#define R2TERR_GENERIC     0x01
#define R2TERR_BADMSG      0x02
#define R2TERR_CONNREFUSED 0x03
#define R2TERR_FORBIDDEN   0x04
#define R2TERR_NOTAVAIL    0x05
#define R2TERR_RESOLVE     0x06
#define R2TERR_NOTFOUND    0x07
#define R2TERR_MAX         0x08

/** generic rdp2tcp message header */
PACK(struct _r2tmsg {
	unsigned char cmd; /**< R2TCMD_xxx */
	unsigned char id;  /**< tunnel identifier */
});
typedef struct _r2tmsg r2tmsg_t;

/** R2TCMD_CONN or R2TCMD_BIND message (client --> server) */
PACK(struct _r2tmsg_connreq {
	unsigned char cmd;   /**< R2TCMD_CONN or R2TCMD_BIND */
	unsigned char id;    /**< tunnel identifier */
	unsigned short port; /**< TCP port or 0 for process tunnel */
	unsigned char af;    /**< address family */
	char hostname[0];    /**< tunnel remote hostname or command line */
});
typedef struct _r2tmsg_connreq r2tmsg_connreq_t;

/** R2TCMD_CONN or R2TCMD_BIND message (server --> client) */
PACK(struct _r2tmsg_connans {
	unsigned char cmd;      /**< R2TCMD_CONN or R2TCMD_BIND */
	unsigned char id;       /**< tunnel identifier */
	unsigned char err;      /**< error code */
	unsigned char af;       /**< address family */
	unsigned short port;    /**< TCP port or 0 for process tunnel */
	unsigned char addr[16]; /**< tunnel address */
});
typedef struct _r2tmsg_connans r2tmsg_connans_t;

/** R2TCMD_RCONN message (server --> client) */
PACK(struct _r2tmsg_rconnreq {
	unsigned char cmd;      /**< R2TCMD_RCONN */
	unsigned char id;       /**< local tunnel identifier */
	unsigned char rid;      /**< remote tunnel identifier */
	unsigned char af;       /**< address family */
	unsigned short port;    /**< TCP port */
	unsigned char addr[16]; /**< tunnel address */
});
typedef struct _r2tmsg_rconnreq r2tmsg_rconnreq_t;

#endif
