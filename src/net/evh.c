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
#include <evh.h>
#include <misc.h>
#include <socket.h>
#include <stdio.h>
#include <sys.h>

#define MAX_EVENTS 128
#define MAX_READ_LEN 512

STATIC int evh_proc_wakeup(int mplex, int wakeup, Channel regqueue) {
	Connection connection;
	char buf[1];
	int rlen = read(wakeup, buf, 1);
	printf("rlen=%i\n", rlen);
	if (rlen <= 0) return -1;
	printf("proc wakeup\n");

	while (!channel_recv_now(&regqueue, &connection)) {
		Connection *conn = alloc(sizeof(Connection));
		*conn = connection;
		printf("recv conn %i\n", connection.socket);
		mregister(mplex, connection.socket, MULTIPLEX_FLAG_READ, conn);
	}

	return 0;
}

STATIC int evh_proc_accept(int mplex, Connection *conn) {
	int acceptfd;
	while (!socket_accept(&conn->socket, &acceptfd)) {
		Connection *nconn = alloc(sizeof(Connection));
		printf("accept a conn\n");
		nconn->conn_type = Inbound;
		nconn->socket = acceptfd;
		nconn->data.inbound.is_closed = false;
		nconn->data.inbound.lock = LOCK_INIT;
		nconn->data.inbound.on_recv = conn->data.acceptor.on_recv;
		nconn->data.inbound.on_close = conn->data.acceptor.on_close;
		nconn->data.inbound.rbuf = NULL;
		nconn->data.inbound.rbuf_capacity = 0;
		nconn->data.inbound.rbuf_offset = 0;
		mregister(mplex, acceptfd, MULTIPLEX_FLAG_READ, nconn);
		conn->data.acceptor.on_accept(NULL, nconn);
	}
	return 0;
}

STATIC int evh_proc_read(Connection *conn) {
	int rlen;
	void *tmp;
	printf("CHECK cap=%lu,off=%lu\n", conn->data.inbound.rbuf_capacity,
	       conn->data.inbound.rbuf_offset);
	if (conn->data.inbound.rbuf_capacity - conn->data.inbound.rbuf_offset <
	    MAX_READ_LEN) {
		printf("RESIZE cap=%lu,off=%lu,nsize=%lu\n",
		       conn->data.inbound.rbuf_capacity,
		       conn->data.inbound.rbuf_offset,
		       conn->data.inbound.rbuf_capacity + MAX_READ_LEN * 2);
		tmp =
		    resize(conn->data.inbound.rbuf,
			   conn->data.inbound.rbuf_capacity + MAX_READ_LEN * 2);
		if (!tmp) return -1;
		conn->data.inbound.rbuf = tmp;
		conn->data.inbound.rbuf_capacity += MAX_READ_LEN * 2;
	}
	rlen = read(conn->socket,
		    conn->data.inbound.rbuf + conn->data.inbound.rbuf_offset,
		    MAX_READ_LEN);
	if (rlen > 0) {
		conn->data.inbound.rbuf_offset += rlen;
		conn->data.inbound.on_recv(NULL, conn, rlen);
	} else {
		printf("close conn!!!!\n");
		conn->data.inbound.on_close(NULL, conn);
		close(conn->socket);
		release(conn->data.inbound.rbuf);
		release(conn);
	}
	printf("proc read1\n");
	return 0;
}

STATIC int evh_read(int mplex, Connection *conn) {
	if (conn->conn_type == Acceptor)
		evh_proc_accept(mplex, conn);
	else
		evh_proc_read(conn);
	return 0;
}

STATIC void evh_loop(int wakeup, Channel regqueue) {
	int mplex = multiplex(), nevents, i;
	Event events[MAX_EVENTS];

	printf("mplex=%i\n", mplex);

	if (mregister(mplex, wakeup, MULTIPLEX_FLAG_READ, &wakeup)) exit(-1);
	while ((nevents = mwait(mplex, events, MAX_EVENTS, -1)) >= 0) {
		printf("nevents=%i\n", nevents);
		for (i = 0; i < nevents; i++) {
			void *attach = event_attachment(events[i]);
			if (attach == &wakeup) {
				if (evh_proc_wakeup(mplex, wakeup, regqueue))
					goto break_loop;
			} else if (event_is_read(events[i])) {
				printf("is_read\n");
				evh_read(mplex, attach);
			}
		}
	}
break_loop:
	printf("exiting evh loop\n");
}

int evh_register(Evh *evh, Connection *connection) {
	int wlen;
	channel_send(&evh->regqueue, connection);
	wlen = write(evh->wakeup, "1", 1);

	if (wlen == -1) return -1;
	return 0;
}

int evh_start(Evh *evh) {
	int fds[2];
	int npid;
	if (pipe(fds)) return -1;
	evh->regqueue = channel(sizeof(Connection), 128);
	if (!channel_ok(&evh->regqueue)) {
		close(fds[0]);
		close(fds[1]);
		return -1;
	}
	evh->wakeup = fds[1];
	if ((npid = fork()) == 0) {
		close(fds[1]);
		evh_loop(fds[0], evh->regqueue);
		exit(0);
	} else if (npid != -1) {
		close(fds[0]);
	} else {
		close(fds[0]);
		close(fds[1]);
		channel_cleanup(&evh->regqueue);
		return -1;
	}

	return 0;
}

int evh_stop(Evh *evh) {
	close(evh->wakeup);
	return 0;
}
