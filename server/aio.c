/**
 * @file aio.c
 * async I/O helpers
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
#include "print.h"

#ifdef DEBUG
extern int debug_level;
#endif

/**
 * initialize async I/O forwarding
 * @param[out] rio pointer to allocated aio_t (reading)
 * @param[out] wio pointer to allocated aio_t (writing)
 * @return 0 on success
 */
int __aio_init_forward(aio_t *rio, aio_t *wio
#ifdef DEBUG
							, const char *name
#endif
							)
{
	HANDLE evt1, evt2;

	assert(rio && wio);

	evt1 = CreateEvent(NULL, TRUE, TRUE, NULL);
	if (!evt1)
		return syserror("CreateEvent");

	evt2 = CreateEvent(NULL, TRUE, TRUE, NULL);
	if (!evt2) {
		CloseHandle(evt1);
		return syserror("CreateEvent");
	}

#ifdef DEBUG
	__iobuf_init2(&rio->buf, &wio->buf, name);
#else
	__iobuf_init2(&rio->buf, &wio->buf);
#endif

	rio->io.hEvent = evt1;
	wio->io.hEvent = evt2;
	rio->min_io_size = 1024;
	wio->min_io_size = 0;

	return 0;
}

/**
 * destroy async I/O forwarding
 * @param[in,out] rio aio_t initialized with aio_init_forward (reading)
 * @param[in,out] wio aio_t initialized with aio_init_forward (writing)
 */
void aio_kill_forward(aio_t *rio, aio_t *wio)
{
	assert(valid_aio(rio) && valid_aio(wio));

	iobuf_kill2(&rio->buf, &wio->buf);
	CloseHandle(rio->io.hEvent);
	CloseHandle(wio->io.hEvent);
}

/**
 * async read from file descriptor to I/O buffer
 * @param[in,out] rio aio_t structure associated with input buffer
 * @param[in] fd file descriptor
 * @param[in] name name of stream to be displayed on console
 * @param[in] callback function called data are received 
 * @param[in] ctx context passed as argument to callback function
 * @return -1 on error
 */
int aio_read(
		aio_t *rio,
		HANDLE fd,
		const char *name,
		aio_readcb_t callback,
		void *ctx)
{
	iobuf_t *ibuf;
	char *data;
	DWORD len, r;
	unsigned int avail, min_io_size;

	assert(valid_aio(rio) && name && *name && callback);
	ibuf = &rio->buf;
	min_io_size = rio->min_io_size;
	len = 0;

	if (rio->pending) {
		rio->pending = 0;
		if (!GetOverlappedResult(fd, &rio->io, &len, FALSE)) {
			ResetEvent(rio->io.hEvent);
			return syserror("GetOverlappedResult");
		}
		
		if (!len) {
			ResetEvent(rio->io.hEvent);
			return error("fd closed");
		}

		if (len == min_io_size) { // increase I/O chunks size (perfs..)
			min_io_size <<= 1;
			if (min_io_size > NETBUF_MAX_SIZE) min_io_size = NETBUF_MAX_SIZE;
			rio->min_io_size = min_io_size;
		}

		print_xfer(name, 'r', (unsigned int) len);
		iobuf_commit(ibuf, len);
		if (callback(ibuf, ctx) < 0) {
			ResetEvent(rio->io.hEvent);
			return -1;
		}
	}

	data = iobuf_reserve(ibuf, min_io_size, &avail);
	if (!data) {
		ResetEvent(rio->io.hEvent);
		return error("failed to allocate %s buffer", name);
	}

	r = 0;
	if (ReadFile(fd, data, (DWORD)avail, &r, &rio->io)) {

		trace_chan("%i/%i overlap=%u", r, avail, len);
		if (r == 0) {
			ResetEvent(rio->io.hEvent);
			return error("fd closed");
		}

		if (r == min_io_size) { // increase I/O chunks size (perfs..)
			min_io_size <<= 1;
			if (min_io_size > NETBUF_MAX_SIZE) min_io_size = NETBUF_MAX_SIZE;
			rio->min_io_size = min_io_size;
		}

		print_xfer(name, 'r', r);
		iobuf_commit(ibuf, (unsigned int)r);
		if (callback(ibuf, ctx) < 0) {
			ResetEvent(rio->io.hEvent);
			return -1;
		}

	} else {

		switch (GetLastError()) {

			case ERROR_IO_PENDING:
				rio->pending = 1;
				break;

			case ERROR_BROKEN_PIPE:
				info(0, "child process has closed pipe");
				break;

			default:
				ResetEvent(rio->io.hEvent);
				return syserror("failed to read");
		}
	}

	return 0;
}

/**
 * async write from I/O buffer to file descriptor
 * @param[in,out] wio aio_t structure associated with output buffer
 * @param[in] fd file descriptor
 * @param[in] name name of stream to be displayed on console
 * @return -1 on error
 */
int aio_write(aio_t *wio, HANDLE fd, const char *name)
{
	iobuf_t *obuf;
	DWORD len, w;

	assert(valid_aio(wio) && name && *name);
	obuf = &wio->buf;

	if (wio->pending) {
		wio->pending = 0;
		len = 0;
		if (!GetOverlappedResult(fd, &wio->io, &len, FALSE)) {
			ResetEvent(wio->io.hEvent);
			return syserror("GetOverlappedResult");
		}
		iobuf_consume(obuf, (unsigned int)len);
		print_xfer(name, 'w', len);
	}

	len = (DWORD) iobuf_datalen(obuf);
	if (len == 0) {
		ResetEvent(wio->io.hEvent);
		return 0;
	}

#ifdef DEBUG
	if (debug_level > 0) iobuf_dump(obuf);
#endif

	w = 0;
	if (WriteFile(fd, iobuf_dataptr(obuf), len, &w, &wio->io)) {

		if (w == 0) {
			ResetEvent(wio->io.hEvent);
			return error("fd closed");
		}

		iobuf_consume(obuf, w);
		print_xfer(name, 'w', w);

	} else {

		switch (GetLastError()) {

			case ERROR_IO_PENDING:
				wio->pending = 1;
				break;

			case ERROR_BROKEN_PIPE:
				info(0, "child process has closed pipe");
				break;

			case ERROR_INVALID_FUNCTION:
				ResetEvent(wio->io.hEvent);
				return error("not running within a TS session");

			default:
				ResetEvent(wio->io.hEvent);
				return syserror("failed to write");
		}
	}

	return 0;
}

