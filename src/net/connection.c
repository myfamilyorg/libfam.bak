/********************************************************************************
 * MIT License
 *
 * Copyright (c) 2025 Christopher Gilliard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/

#include <alloc.h>
#include <error.h>
#include <event.h>
#include <evh.h>
#include <lock.h>
#include <misc.h>
#include <syscall.h>
#include <syscall_const.h>

int connection_close(Connection *connection) {
	LockGuard lg = wlock(&connection->data.inbound.lock);
	InboundData *ib = &connection->data.inbound;

	ib->is_closed = true;
	shutdown(connection->socket, SHUT_RDWR);

	return 0;
}
int connection_write(Connection *connection, const uint8_t *buf, size_t len) {
	ssize_t wlen = 0;
	InboundData *ib = &connection->data.inbound;
	LockGuard lg = wlock(&ib->lock);
	if (ib->is_closed) return -1;
	if (!ib->wbuf_offset) {
	write_block:
		err = 0;
		wlen = write(connection->socket, buf, len);
		if (err == EINTR)
			goto write_block;
		else if (err == EAGAIN)
			wlen = 0; /* Set for other logic */
		else if (err) {	  /* shutdown for other errors */
			shutdown(connection->socket, SHUT_RDWR);
			ib->is_closed = true;
			return -1;
		}

		if ((size_t)wlen == len) return 0;
		mregister(ib->mplex, connection->socket,
			  MULTIPLEX_FLAG_READ | MULTIPLEX_FLAG_WRITE,
			  connection);
	}

	if (ib->wbuf_offset + len - wlen > ib->wbuf_capacity) {
		void *tmp = resize(ib->wbuf, ib->wbuf_offset + len - wlen);
		if (!tmp) {
			shutdown(connection->socket, SHUT_RDWR);
			ib->is_closed = true;
			return -1;
		}
		ib->wbuf = tmp;
		ib->wbuf_capacity = ib->wbuf_offset + len - wlen;
	}
	memcpy(ib->wbuf + ib->wbuf_offset, buf + wlen, len - wlen);
	ib->wbuf_offset += len - wlen;

	return 0;
}

