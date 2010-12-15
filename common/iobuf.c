/**
 * @file iobuf.c
 * @brief I/O buffer helpers
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
#include "debug.h"
#include "iobuf.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#ifdef DEBUG
#include <stdio.h>
#endif

/**
 * @brief initialize I/O buffer
 * @param[out] buf buffer to initialize
 */
void __iobuf_init(iobuf_t *buf
#ifdef DEBUG
			, char type, const char *name
#endif
)
{
	assert(buf);

	buf->data  = NULL;
	buf->size  = 0;
	buf->total = 0;
#ifdef DEBUG
	buf->name  = name;
	buf->type  = type;
#endif

	trace_iobuf("[%c] %s", type, name);
}


/**
 * initialize 2 I/O buffers
 * @param[out] ibuf input buffer
 * @param[out] obuf output buffer
 */
void __iobuf_init2(iobuf_t *ibuf, iobuf_t *obuf
#ifdef DEBUG
							, const char *name
#endif
							)
{
	assert(ibuf && obuf);
	trace_iobuf("%s", name);

#ifdef DEBUG
	__iobuf_init(ibuf, 'r', name);
	__iobuf_init(obuf, 'w', name);
#else
	__iobuf_init(ibuf);
	__iobuf_init(obuf);
#endif
}

/**
 * destroy an I/O buffer
 * @param[in] buf buffer to destroy
 */
void iobuf_kill(iobuf_t *buf)
{
	assert_iobuf(buf);
	trace_iobuf("[%c] %s", buf->type, buf->name);

	if (buf->data)
		free(buf->data);
}

/**
 * destroy 2 I/O buffers
 * @param[in] ibuf input buffer
 * @param[in] obuf output buffer
 */
void iobuf_kill2(iobuf_t *ibuf, iobuf_t *obuf)
{
	assert(valid_iobuf(ibuf) && valid_iobuf(obuf));

	iobuf_kill(obuf);
	iobuf_kill(ibuf);
}

/**
 * @brief consume data in an I/O buffer
 * @param[in] buf I/O buffer where data will be consumed
 * @param[in] consumed size of consumed data
 */
void iobuf_consume(iobuf_t *buf, unsigned int consumed)
{
	unsigned int size;

	assert(valid_iobuf(buf) && (consumed > 0) && (consumed <= buf->size));

	size = buf->size - consumed;
	trace_iobuf("[%c] %s, consumed=%u, remaining=%u",
					buf->type, buf->name, consumed, size);

	if (size)
		memmove(buf->data, buf->data + consumed, size);

	buf->size = size;
}

/**
 * @brief reserve space in an I/O buffer
 * @param[in] buf I/O buffer where data will be written
 * @param[in] size size to reserve
 * @param[out] reserved will hold the size of allocated data
 * @return pointer where data have been allocated
 * @note if size is 0 reserved must be non-NULL
 */
void *iobuf_reserve(iobuf_t *buf, unsigned int size, unsigned int *reserved)
{
	unsigned int avail;
	void *bak, *data;

	assert(valid_iobuf(buf) && (size || reserved));

	avail = buf->total - buf->size;

	if (!size)
		size = IOBUF_MIN_SIZE;

	trace_iobuf("[%c] %s, size=%u, avail=%u",
					buf->type, buf->name, size, avail);

	if (size > avail) {
		bak = buf->data;
		data = realloc(bak, buf->size + size);
		if (!data)
			return NULL;
		buf->data = data;
		buf->total = buf->size + size;
	}

	if (reserved)
		*reserved = size;

	return buf->data + buf->size;
}

/**
 * commit data to an I/O buffer
 * @param[in] buf I/O buffer where data have been written
 * @param[in] commited size of data to commit
 * @note data must have been previously allocated with iobuf_reserve
 */
void iobuf_commit(iobuf_t *buf, unsigned int commited)
{
	assert(valid_iobuf(buf) && (commited > 0)
				&& (commited <= (buf->total - buf->size)));
	trace_iobuf("[%c] %s, commited=%u, total=%u, size=%u",
			buf->type, buf->name, commited, buf->total, buf->size);

	buf->size += commited;
}

/**
 * append data to an I/O buffer
 * @param[in] buf I/O buffer to hold data
 * @param[in] data content to append
 * @param[in] size size of data to append
 * @return pointer where data have been written or NULL if memory
 *         cannot be allocated
 */
void *iobuf_append(iobuf_t *buf, const void *data, unsigned int size)
{
	void *ptr;

	assert(valid_iobuf(buf) && data && size);
	trace_iobuf("[%c] %s, size=%u", buf->type, buf->name, size);

	ptr = iobuf_reserve(buf, size, NULL);
	if (!ptr)
		return NULL;
	memcpy(ptr, data, size);
	iobuf_commit(buf, size);

	return ptr;
}

#ifdef DEBUG
void iobuf_dump(iobuf_t *buf)
{
	unsigned int i, len;
	unsigned char *data;

	data = (unsigned char *)iobuf_dataptr(buf);
	fprintf(stderr, "[%s-%c] ", buf->name, buf->type);
	for (i=0, len=iobuf_datalen(buf); i<len; ++i)
		fprintf(stderr, "%02x", data[i]);
	fputc('\n', stderr);
}
#endif

// vim: ts=3 sw=3
