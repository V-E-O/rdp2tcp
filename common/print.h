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
#ifndef __PRINT_H__
#define __PRINT_H__

#include <stdio.h>
#include "debug.h"

void print_init(void);

void info(int, const char *, ...);
int warn(const char *, ...);
int error(const char *, ...);

void print_xfer(const char *, char, unsigned int);

#ifdef DEBUG
void fprint_hex(void *, unsigned int, FILE *);
#endif

#endif
// vim: ts=3 sw=3
