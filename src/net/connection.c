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
#include <socket.h>
#include <syscall.h>
#include <syscall_const.h>

Connection *evh_acceptor(uint8_t addr[4], uint16_t port, uint16_t backlog,
			 OnRecvFn on_recv_fn, OnAcceptFn on_accept_fn,
			 OnCloseFn on_close_fn) {
	int pval;
	Connection *conn = alloc(sizeof(Connection));
	if (conn == NULL) return NULL;
	conn->conn_type = Acceptor;
	conn->data.acceptor.on_accept = on_accept_fn;
	conn->data.acceptor.on_recv = on_recv_fn;
	conn->data.acceptor.on_close = on_close_fn;
	pval = socket_listen(&conn->socket, addr, port, backlog);
	if (pval == -1) {
		release(conn);
		return NULL;
	}
	conn->data.acceptor.port = pval;
	return conn;
}
uint16_t evh_acceptor_port(Connection *conn) {
	if (conn->conn_type == Acceptor) return conn->data.acceptor.port;
	err = EINVAL;
	return 0;
}

Connection *evh_client(uint8_t addr[4], uint16_t port, OnRecvFn on_recv_fn,
		       OnCloseFn on_close_fn) {
	Connection *client = alloc(sizeof(Connection));
	if (client == NULL) return NULL;
	client->conn_type = Outbound;
	client->data.inbound.on_recv = on_recv_fn;
	client->data.inbound.on_close = on_close_fn;
	client->data.inbound.lock = LOCK_INIT;
	client->data.inbound.is_closed = false;
	client->data.inbound.rbuf = NULL;
	client->data.inbound.rbuf_capacity = 0;
	client->data.inbound.rbuf_offset = 0;
	client->data.inbound.wbuf = NULL;
	client->data.inbound.wbuf_capacity = 0;
	client->data.inbound.wbuf_offset = 0;
	client->socket = socket_connect(addr, port);
	if (client->socket == -1) {
		release(client);
		return NULL;
	}
	return client;
}

int connection_close(Connection *connection) {
	LockGuard lg = wlock(&connection->data.inbound.lock);
	InboundData *ib = &connection->data.inbound;
	if (ib->is_closed) return -1;
	ib->is_closed = true;
	return shutdown(connection->socket, SHUT_RDWR);
}
int connection_write(Connection *connection, const void *buf, size_t len) {
	ssize_t wlen = 0;
	InboundData *ib = &connection->data.inbound;
	LockGuard lg = wlock(&ib->lock);
	if (ib->is_closed) return -1;
	if (!ib->wbuf_offset) {
		size_t offset = 0;
	write_block:
		err = 0;
		wlen = write(connection->socket, (uint8_t *)buf + offset, len);
		if (err == EINTR) {
			if (wlen > 0) offset += wlen;
			goto write_block;
		} else if (err == EAGAIN)
			wlen = 0; /* Set for other logic */
		else if (err) {	  /* shutdown for other errors */
			shutdown(connection->socket, SHUT_RDWR);
			ib->is_closed = true;
			return -1;
		}

		if ((size_t)wlen == len) return 0;
		if (mregister(ib->mplex, connection->socket,
			      MULTIPLEX_FLAG_READ | MULTIPLEX_FLAG_WRITE,
			      connection) == -1) {
			shutdown(connection->socket, SHUT_RDWR);
			ib->is_closed = true;
			return -1;
		}
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
	memcpy(ib->wbuf + ib->wbuf_offset, (uint8_t *)buf + wlen, len - wlen);
	ib->wbuf_offset += len - wlen;

	return 0;
}

void connection_clear_rbuf_through(Connection *conn, size_t off) {
	InboundData *ib = &conn->data.inbound;
	if (off > ib->rbuf_offset) return;
	memorymove(ib->rbuf, ib->rbuf + off, ib->rbuf_offset - off);
	ib->rbuf_offset -= off;
}

void connection_clear_rbuf(Connection *conn) {
	InboundData *ib = &conn->data.inbound;
	ib->rbuf_offset = 0;
}
