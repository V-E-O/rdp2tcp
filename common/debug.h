/**
 * @file debug.h
 * debug/trace support
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
#ifndef __DEBUG_H__
#define __DEBUG_H__

#if defined(_DEBUG) && !defined(DEBUG)
#define DEBUG
#endif

#ifndef DEBUG
#define NDEBUG
#endif

#include <assert.h>

#ifdef DEBUG
#define debug       __debug
void __debug(int, const char *, ...);

extern int tracing_flags;

void __trace(const char *, int, const char *, const char *, ...);

#define trace(cat, ...) \
	{ if (tracing_flags & (1 << (cat))){ \
		__trace(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__);} }
	
#define LIB_TRACING_CATS \
		"iobuf", "sock", "chan", "evt", "proc", "ctrl", "tun", "socks"

#else
/** print debug statement */
#define debug(a, ...) ((void)0)
/** generate call trace */
#define trace(...)    ((void)0)
#endif

#define trace_iobuf(...) trace(0, __VA_ARGS__)
#define trace_sock(...)  trace(1, __VA_ARGS__)
#define trace_chan(...)  trace(2, __VA_ARGS__)
#define trace_evt(...)   trace(3, __VA_ARGS__)
#define trace_proc(...)  trace(4, __VA_ARGS__)
#define trace_ctrl(...)  trace(5, __VA_ARGS__)
#define trace_tun(...)   trace(6, __VA_ARGS__)
#define trace_socks(...) trace(7, __VA_ARGS__)

#endif
