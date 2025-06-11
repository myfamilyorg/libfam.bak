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
#include <atomic.h>
#include <error.h>
#include <event.h>
#include <evh.h>
#include <misc.h>
#include <socket.h>
#include <sys.h>
#include <syscall.h>
#include <syscall_const.h>

#define MAX_EVENTS 128
#define MIN_EXCESS 1024
#define MIN_RESIZE (MIN_EXCESS * 2)

/* marker for wakeup */
Connection wakeup_attachment = {0};

STATIC int proc_wakeup(int wakeup) {
	char buf[1];
	int v;
	v = read(wakeup, buf, 1);
	if (v <= 0) return -1;
	return 0;
}

STATIC int proc_acceptor(Evh *evh, Connection *acceptor, void *ctx) {
	while (true) {
		Connection *nconn;
		int fd = socket_accept(acceptor->socket);
		if (fd == -1) {
			if (err != EAGAIN) perror("socket_accept");
			break;
		}
		nconn =
		    alloc(sizeof(Connection) + evh->connection_alloc_overhead);
		if (nconn == NULL) {
			/* Cannot allocate memory, drop connection */
			close(fd);
			continue;
		}

		nconn->conn_type = Inbound;
		nconn->socket = fd;
		nconn->data.inbound.on_recv = acceptor->data.acceptor.on_recv;
		nconn->data.inbound.on_close = acceptor->data.acceptor.on_close;
		nconn->data.inbound.lock = LOCK_INIT;
		nconn->data.inbound.is_closed = false;
		nconn->data.inbound.rbuf_capacity = 0;
		nconn->data.inbound.rbuf_offset = 0;
		nconn->data.inbound.wbuf_capacity = 0;
		nconn->data.inbound.wbuf_offset = 0;
		nconn->data.inbound.wbuf = nconn->data.inbound.rbuf = NULL;
		if (evh_register(evh, nconn) == -1) {
			perror("evh_register");
			close(fd);
			release(nconn);
			continue;
		}

		acceptor->data.acceptor.on_accept(ctx, nconn);
	}
	return 0;
}

STATIC int check_capacity(Connection *conn) {
	InboundData *ib = &conn->data.inbound;
	if (ib->rbuf_offset + MIN_EXCESS > ib->rbuf_capacity) {
		void *tmp = resize(ib->rbuf, ib->rbuf_offset + MIN_RESIZE);
		if (tmp == NULL) return -1;
		ib->rbuf = tmp;
		ib->rbuf_capacity = ib->rbuf_offset + MIN_RESIZE;
	}

	return 0;
}

STATIC int proc_close(Connection *conn, void *ctx) {
	int res;
	InboundData *ib = &conn->data.inbound;
	LockGuard lg;

	ib->on_close(ctx, conn);
	if (ib->rbuf_capacity) release(ib->rbuf);
	ib->rbuf_capacity = 0;
	ib->rbuf_offset = 0;

	{
		lg = wlock(&ib->lock);
		ib->is_closed = true;
		if (ib->wbuf_capacity) release(ib->wbuf);
		ib->wbuf_capacity = 0;
		ib->wbuf_offset = 0;
	}
	res = close(conn->socket);
	release(conn);
	return res;
}

STATIC int proc_read(Connection *conn, void *ctx) {
	while (true) {
		InboundData *ib;
		ssize_t rlen;

		if (check_capacity(conn) == -1) {
			proc_close(conn, ctx);
			break;
		}
		ib = &conn->data.inbound;
		err = 0;
		rlen = read(conn->socket, ib->rbuf + ib->rbuf_offset,
			    ib->rbuf_capacity - ib->rbuf_offset);

		if (rlen <= 0) {
			if (err != EAGAIN) proc_close(conn, ctx);
			break;
		}

		ib->rbuf_offset += rlen;
		ib->on_recv(ctx, conn, rlen);
	}
	return 0;
}

STATIC int proc_write(Evh *evh, Connection *conn,
		      void *ctx __attribute__((unused))) {
	InboundData *ib = &conn->data.inbound;
	ssize_t wlen;
	size_t cur = 0;
	int sock = conn->socket;
	LockGuard lg = wlock(&ib->lock);

	while (cur < ib->wbuf_offset) {
		wlen = write(sock, ib->wbuf + cur, ib->wbuf_offset - cur);
		if (wlen < 0) {
			shutdown(sock, SHUT_RDWR);
			return -1;
		}
		cur += wlen;
	}

	if (cur == ib->wbuf_offset) {
		if (mregister(evh->mplex, sock, MULTIPLEX_FLAG_READ, conn) ==
		    -1) {
			shutdown(sock, SHUT_RDWR);
			return -1;
		}
		if (ib->wbuf_capacity) {
			release(ib->wbuf);
			ib->wbuf_offset = 0;
			ib->wbuf_capacity = 0;
		}
	} else {
		memorymove(ib->wbuf, ib->wbuf + cur, ib->wbuf_offset - cur);
		ib->wbuf_offset -= cur;
	}

	return 0;
}

STATIC void event_loop(Evh *evh, void *ctx, int wakeup) {
	int i, count;
	Event events[MAX_EVENTS];

	while (true) {
		count = mwait(evh->mplex, events, MAX_EVENTS, -1);
		for (i = 0; i < count; i++) {
			Connection *conn = event_attachment(events[i]);
			if (conn == &wakeup_attachment) {
				if (proc_wakeup(wakeup) == -1) goto end_while;
			} else {
				if (conn->conn_type == Acceptor) {
					proc_acceptor(evh, conn, ctx);
				} else {
					if (event_is_write(events[i]))
						proc_write(evh, conn, ctx);
					if (event_is_read(events[i]))
						proc_read(conn, ctx);
				}
			}
		}
	}
end_while:
	close(evh->mplex);
	close(wakeup);
	ASTORE(evh->stopped, 1);
}

STATIC int init_event_loop(Evh *evh, void *ctx) {
	int fds[2];
	pid_t pid;
	if (pipe(fds) == -1) {
		return -1;
	}

	evh->wakeup = fds[1];

	set_nonblocking(fds[0]);

	if (mregister(evh->mplex, fds[0], MULTIPLEX_FLAG_READ,
		      &wakeup_attachment) == -1) {
		close(fds[1]);
		close(fds[0]);
		return -1;
	}

	pid = two();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		return -1;
	}
	if (pid == 0) {
		/* Child process */
		event_loop(evh, ctx, fds[0]);
		exit(0);
	}

	return 0;
}

int evh_register(Evh *evh, Connection *connection) {
	if (connection->conn_type == Acceptor)
		return mregister(evh->mplex, connection->socket,
				 MULTIPLEX_FLAG_ACCEPT, connection);
	else {
		connection->data.inbound.mplex = evh->mplex;
		return mregister(evh->mplex, connection->socket,
				 MULTIPLEX_FLAG_READ, connection);
	}
}

int evh_start(Evh *evh, void *ctx, uint64_t connection_alloc_overhead) {
	evh->mplex = multiplex();
	evh->connection_alloc_overhead = connection_alloc_overhead;
	evh->stopped = alloc(sizeof(uint64_t));
	if (evh->stopped == NULL) {
		close(evh->mplex);
		return -1;
	}
	*(evh->stopped) = false;
	if (evh->mplex == -1) return -1;
	if (init_event_loop(evh, ctx) == -1) {
		close(evh->mplex);
		release(evh->stopped);
		return -1;
	}

	return 0;
}

int evh_stop(Evh *evh) {
	if (close(evh->wakeup) == -1) return -1;
	while (!ALOAD(evh->stopped)) yield();
	release(evh->stopped);
	return 0;
}

