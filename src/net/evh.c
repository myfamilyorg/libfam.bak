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

#include <alloc.H>
#include <atomic.H>
#include <error.H>
#include <event.H>
#include <evh.H>
#include <format.H>
#include <socket.H>
#include <sys.H>
#include <syscall_const.H>

#define MAX_EVENTS 128

#ifdef COVERAGE
void __gcov_dump(void);
#endif /* COVERAGE */

i32 wakeup_attachment = 0;

struct Evh {
	i32 wakeup;
	i32 mplex;
	i32 stopped;
};

STATIC i32 proc_wakeup(i32 wakeup) {
	u8 buf[1];
	i32 v;
	v = read(wakeup, buf, 1);
	if (v <= 0) return -1;
	return 0;
}

STATIC void proc_acceptor(Evh *evh, Connection *acceptor, void *ctx) {
	while (true) {
		Connection *nconn;
		i32 acceptfd = connection_socket(acceptor);
		i32 fd = socket_accept(acceptfd);
		if (fd < 0) {
			if (err != EAGAIN) perror("socket_accept");
			break;
		}

		nconn = evh_accepted(fd, connection_on_recv(acceptor),
				     connection_on_close(acceptor),
				     connection_alloc_overhead(acceptor));
		if (!nconn || evh_register(evh, nconn) < 0) {
			close(fd);
			if (nconn) release(nconn);
			continue;
		}
		connection_on_accept(acceptor)(ctx, nconn);
	}
}

STATIC void proc_close(Connection *conn, void *ctx) {
	connection_on_close(conn)(conn, ctx);
	connection_release(conn);
}

STATIC void proc_read(Connection *conn, void *ctx) {
	i64 rlen = 0;
	i32 socket;
	u8 *rbuf;
	u64 capacity, offset;

	while (true) {
		if (connection_check_capacity(conn) < 0) {
			proc_close(conn, ctx);
			break;
		}

		rbuf = connection_rbuf(conn);
		capacity = connection_rbuf_capacity(conn);
		offset = connection_rbuf_offset(conn);
		socket = connection_socket(conn);

		err = 0;
		rlen = read(socket, rbuf + offset, capacity - offset);

		if (rlen <= 0) {
			if (err != EAGAIN) proc_close(conn, ctx);
			break;
		}
		connection_set_rbuf_offset(conn, offset + rlen);
		connection_on_recv(conn)(ctx, conn, rlen);
	}
}

STATIC i32 proc_write(Evh *evh, Connection *conn) {
	if (evh || conn) {
	}
	/*
	InboundData *ib = &conn->data.inbound;
	i64 wlen;
	u64 cur = 0;
	i32 sock = conn->socket;
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
	*/

	return 0;
}

STATIC void event_loop(Evh *evh, void *ctx, i32 wakeup) {
	i32 i, count;
	Event events[MAX_EVENTS];

	while (true) {
		count = mwait(evh->mplex, events, MAX_EVENTS, -1);
		for (i = 0; i < count; i++) {
			Connection *conn = event_attachment(events[i]);
			if (conn == (Connection *)&wakeup_attachment) {
				if (proc_wakeup(wakeup) == -1) goto end_while;
			} else {
				if (connection_type(conn) == Acceptor) {
					proc_acceptor(evh, conn, ctx);
				} else {
					if (event_is_write(events[i]))
						proc_write(evh, conn);
					if (event_is_read(events[i]))
						proc_read(conn, ctx);
				}
			}
		}
	}
end_while:
	close(evh->mplex);
	close(wakeup);
#ifdef COVERAGE
	/* dump coverage info before allowing parent to proceed */
	__gcov_dump();
#endif
	ASTORE(&evh->stopped, getpid());
}

STATIC i32 init_event_loop(Evh *evh, void *ctx) {
	i32 fds[2];
	i32 pid;
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

i32 evh_register(Evh *evh, Connection *conn) {
	i32 socket = connection_socket(conn);
	if (connection_type(conn) == Acceptor) {
		return mregister(evh->mplex, socket, MULTIPLEX_FLAG_ACCEPT,
				 conn);
	} else {
		/* TODO: register as write for client connect */
		connection_set_mplex(conn, evh->mplex);
		return mregister(evh->mplex, socket, MULTIPLEX_FLAG_READ, conn);
	}
}

Evh *evh_start(void *ctx) {
	Evh *evh = alloc(sizeof(Evh));
	if (!evh) return NULL;
	evh->mplex = multiplex();
	if (evh->mplex == -1) {
		release(evh);
		return NULL;
	}
	evh->stopped = 0;
	if (init_event_loop(evh, ctx) == -1) {
		close(evh->mplex);
		release(evh);
		return NULL;
	}
	return evh;
}

i32 evh_stop(Evh *evh) {
	if (ALOAD(&evh->stopped)) return -1;
	if (close(evh->wakeup) == -1) return -1;
	while (!ALOAD(&evh->stopped)) yield();
	waitid(P_PID, evh->stopped, NULL, WEXITED);
	release(evh);
	return 0;
}

