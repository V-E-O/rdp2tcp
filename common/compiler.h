/**
 * @file compiler.h
 * Microsoft Visual Studio compiler support
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
#ifndef __COMPILER_H__
#define __COMPILER_H__

#ifdef __GNUC__
// gcc
#define PACK(decl) decl __attribute__((__packed__))
#else
#define PACK(decl) __pragma(pack(push,1)) decl __pragma(pack(pop))
// Visual Studio
#if defined(_DEBUG) && !defined(DEBUG)
#define DEBUG
#endif
#define ssize_t int
#define inline __inline
#define snprintf _snprintf
#define typeof(x) void *
#endif

#endif
