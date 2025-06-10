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
#include <socket.h>
#include <stdio.h>
#include <syscall_const.h>
#include <test.h>

Test(socket_connect) {
	char buf[10] = {0};
	int server = -1;
	int port = socket_listen(&server, (uint8_t[]){127, 0, 0, 1}, 0, 10);
	int conn = socket_connect((uint8_t[]){127, 0, 0, 1}, port);

	write(conn, "test", 4);
	int inbound = socket_accept(server);
	ASSERT_EQ(read(inbound, buf, 10), 4, "read");
	ASSERT_EQ(buf[0], 't', "t");
	ASSERT_EQ(buf[1], 'e', "e");
	ASSERT_EQ(buf[2], 's', "s");
	ASSERT_EQ(buf[3], 't', "t");

	shutdown(inbound, SHUT_RDWR);
	close(inbound);
	close(server);
	close(conn);
}

typedef struct {
	int fd;
	int v;
} ConnectionInfo;

Test(multi_socket) {
	char buf[10];
	int server, inbound, client, mplex, port;
	Event events[10];

	mplex = multiplex();
	port = socket_listen(&server, (uint8_t[]){127, 0, 0, 1}, 0, 10);
	client = socket_connect((uint8_t[]){127, 0, 0, 1}, port);
	mregister(mplex, server, MULTIPLEX_FLAG_ACCEPT, &server);

	if (two()) {
		bool exit = false;
		while (!exit) {
			err = 0;
			int v = mwait(mplex, events, 10, -1);
			for (int i = 0; i < v; i++) {
				if (event_attachment(events[i]) == &server) {
					inbound = socket_accept(server);
					mregister(mplex, inbound,
						  MULTIPLEX_FLAG_READ,
						  &inbound);
				} else if (event_attachment(events[i]) ==
					   &inbound) {
					ASSERT_EQ(read(inbound, buf, 10), 1,
						  "inb read");
					ASSERT_EQ(buf[0], 'h', "h");
					ASSERT(event_is_read(events[i]),
					       "is_read");
					ASSERT(!event_is_write(events[i]),
					       "is_write");
					ASSERT(!mregister(mplex, inbound,
							  MULTIPLEX_FLAG_WRITE,
							  NULL),
					       "write reg");
					exit = true;
					break;
				}
			}
		}

		close(client);
		close(mplex);
		close(inbound);
		close(server);
	} else {
		while (write(client, "h", 1) != 1);
		exit(0);
	}
}

uint32_t acc = 0;
int on_accept(void *ctx, Connection *conn) {
	__add32(&acc, 1);
	return 0;
}

Evh evh1;

int on_recv(void *ctx, Connection *conn, size_t rlen) {
	char buf[1024 * 64];
	InboundData *ib = &conn->data.inbound;
	memcpy(buf, ib->rbuf, ib->rbuf_offset);
	buf[ib->rbuf_offset] = 0;

	LockGuard lg = wlock(&ib->lock);
	ib->wbuf_capacity = 1024;
	ib->wbuf_offset = 11;
	ib->wbuf = alloc(1024);
	memcpy(ib->wbuf, "0123456789\n", 11);
	evh_wpend(&evh1, conn);

	return 0;
}

int on_close(void *ctx, Connection *conn) { return 0; }

Test(evh1) {
	ASSERT(!evh_start(&evh1, NULL), "evh_start");
	Connection *conn = alloc(sizeof(Connection));
	conn->conn_type = Acceptor;
	conn->data.acceptor.on_accept = on_accept;
	conn->data.acceptor.on_recv = on_recv;
	conn->data.acceptor.on_close = on_close;
	socket_listen(&conn->socket, (uint8_t[]){127, 0, 0, 1}, 9090, 10);
	evh_register(&evh1, conn);

	int tconn = socket_connect((uint8_t[]){127, 0, 0, 1}, 9090);
	write(tconn, "test1\n", 6);

	char buf[100];
	int total = 0;
	while (total < 11) {
		err = 0;
		int x = read(tconn, buf + total, 100 - total);
		if (err == EAGAIN) continue;
		ASSERT(x >= 0, "x>=0");
		total += x;
	}
	ASSERT_EQ(total, 11, "11");
	ASSERT_EQ(buf[0], '0', "0");
	ASSERT_EQ(buf[1], '1', "1");
	evh_stop(&evh1);
	close(conn->socket);
	close(tconn);
	release(conn);
	// ASSERT_EQ(ALOAD(&acc), 1, "acc=1");
}
