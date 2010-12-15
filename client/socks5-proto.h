/**
 * @file socks5-proto.h
 * SOCKS5 protocol specifications
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
#ifndef __SOCKS5_PROTO_H__
#define __SOCKS5_PROTO_H__

// http://tools.ietf.org/html/rfc1928

#define SOCKS5_VERSION     0x05

#define SOCKS5_ATYPE_IPV4  0x01
#define SOCKS5_ATYPE_FQDN  0x03
#define SOCKS5_ATYPE_IPV6  0x04

#define SOCKS5_NOAUTH      0x00

#define SOCKS5_CONNECT     0x01
#define SOCKS5_BIND        0x02
#define SOCKS5_UDPASSOC    0x03

#define SOCKS5_SUCCESS     0x00
#define SOCKS5_ERROR       0x01
#define SOCKS5_FORBIDDEN   0x02
#define SOCKS5_NETUNREACH  0x03
#define SOCKS5_PORTUNREACH 0x04
#define SOCKS5_CONNREFUSED 0x05
#define SOCKS5_TTLEXPIRED  0x06
#define SOCKS5_UNKCOMMAND  0x07
#define SOCKS5_UNKADDRTYPE 0x08

#endif
