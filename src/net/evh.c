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
		nconn->conn_type = Inbound;
		nconn->socket = acceptfd;
		nconn->data.inbound.is_closed = false;
		nconn->data.inbound.lock = LOCK_INIT;
		mregister(mplex, acceptfd, MULTIPLEX_FLAG_READ, nconn);
	}
	return 0;
}

STATIC int evh_proc_read(Connection *conn) { return 0; }

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
			if (attach == &wakeup &&
			    evh_proc_wakeup(mplex, wakeup, regqueue))
				goto break_loop;
			else if (event_is_read(events[i])) {
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
