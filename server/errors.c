/**
 * @file errors.c
 * windows error printing
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

#include <stdio.h>
#include <windows.h>


static int do_error(const char *func, DWORD err)
{
	char *buffer;

	buffer = NULL;
	if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM
						|FORMAT_MESSAGE_ALLOCATE_BUFFER
						|FORMAT_MESSAGE_MAX_WIDTH_MASK
						|FORMAT_MESSAGE_IGNORE_INSERTS,
						NULL, err,
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						(LPSTR)&buffer, 0, NULL) > 0) {
		if (buffer) {
			error("%s (%lu: %s)\n", func, err, buffer);
			LocalFree(buffer);
			return -1;
		}
	}

	return error("%s (%lu)\n", func, err);
}

/** print winsock-level error */
int wsaerror(const char *func)
{
	return do_error(func, WSAGetLastError());
}

/** print win32-level error */
int syserror(const char *func)
{
	return do_error(func, GetLastError());
}

