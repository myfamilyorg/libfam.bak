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
#include <ws.h>

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

uint64_t *value;
Evh evh1;

int on_accept(void *ctx, Connection *conn) {
	__add64(value, 1);
	return 0;
}

int on_recv(void *ctx, Connection *conn, size_t rlen) {
	char buf[1024 * 64];
	InboundData *ib = &conn->data.inbound;
	memcpy(buf, ib->rbuf, ib->rbuf_offset);
	buf[ib->rbuf_offset] = 0;
	connection_write(conn, "0123456789\n", 11);
	connection_close(conn);
	return 0;
}

int on_close(void *ctx, Connection *conn) { return 0; }

Test(test_evh1) {
	ASSERT_BYTES(0);
	int port;
	value = alloc(sizeof(uint64_t));
	*value = 0;

	ASSERT(!evh_start(&evh1, NULL, 0), "evh_start");
	Connection *conn = evh_acceptor((uint8_t[]){127, 0, 0, 1}, 0, 10,
					on_recv, on_accept, on_close);
	port = evh_acceptor_port(conn);

	evh_register(&evh1, conn);

	int tconn = socket_connect((uint8_t[]){127, 0, 0, 1}, port);
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
	ASSERT_EQ(ALOAD(value), 1, "value=1");
	release(value);
	ASSERT_BYTES(0);
}

Evh evh2;
int *value2;

int on_accept2(void *ctx, Connection *conn) { return 0; }

int on_recv2(void *ctx, Connection *conn, size_t rlen) {
	ASSERT_EQ(rlen, sizeof(int), "sizeof(int)");
	InboundData *ib = &conn->data.inbound;
	int *v = (int *)(ib->rbuf + ib->rbuf_offset - rlen);
	int nv = *v + 1;
	ASTORE(value2, nv);
	if (nv < 105)
		connection_write(conn, (uint8_t *)&nv, sizeof(int));
	else
		connection_close(conn);
	return 0;
}

int on_close2(void *ctx, Connection *conn) { return 0; }

Test(test_evh2) {
	ASSERT_BYTES(0);

	int port;
	value2 = alloc(sizeof(int));
	*value2 = 0;
	ASSERT(!evh_start(&evh2, NULL, 0), "evh_start");

	Connection *conn = evh_acceptor((uint8_t[]){127, 0, 0, 1}, 0, 10,
					on_recv2, on_accept2, on_close2);

	port = evh_acceptor_port(conn);
	evh_register(&evh2, conn);

	Connection *client =
	    evh_client((uint8_t[]){127, 0, 0, 1}, port, on_recv2, on_close2, 0);
	ASSERT(!evh_acceptor_port(client), "evh_acceptor_port on client err");
	ASSERT(client->socket > 0, "connect");
	evh_register(&evh2, client);

	int initial = 101;
	ASSERT_EQ(*value2, 0, "0");
	connection_write(client, (uint8_t *)&initial, sizeof(int));
	while (true) {
		yield();
		if (ALOAD(value2) == 105) break;
	}

	ASSERT_EQ(ALOAD(value2), 105, "105");
	connection_close(client);
	evh_stop(&evh2);
	release(conn);
	release(value2);

	ASSERT_BYTES(0);
}

Test(test_evh_coverage) {
	Connection *conn = evh_acceptor((uint8_t[]){127, 0, 0, 1}, 0, 10,
					on_recv2, on_accept2, on_close2);
	uint16_t port = evh_acceptor_port(conn);
	ASSERT(!evh_acceptor((uint8_t[]){127, 0, 0, 1}, port, 10, on_recv2,
			     on_accept2, on_close2),
	       "listen on same port");
	close(conn->socket);
	release(conn);
	ASSERT(!evh_client((uint8_t[]){127, 0, 0, 1}, port, on_recv2, on_close2,
			   0),
	       "connect failure");

	ASSERT_BYTES(0);
}

Test(test_evh_other_situations) {
	Connection *conn = evh_acceptor((uint8_t[]){127, 0, 0, 1}, 0, 10,
					on_recv2, on_accept2, on_close2);
	uint16_t port = evh_acceptor_port(conn);

	Connection *client =
	    evh_client((uint8_t[]){127, 0, 0, 1}, port, on_recv2, on_close2, 0);

	ASSERT(!connection_close(client), "conn_close");

	close(conn->socket);
	release(conn);
	release(client);

	ASSERT_BYTES(0);
}

Test(test_evh_other_situations2) {
	Connection *conn = evh_acceptor((uint8_t[]){127, 0, 0, 1}, 0, 10,
					on_recv2, on_accept2, on_close2);
	uint16_t port = evh_acceptor_port(conn);

	Connection *client =
	    evh_client((uint8_t[]){127, 0, 0, 1}, port, on_recv2, on_close2, 0);
	ASSERT(client, "client");

	close(conn->socket);
	ASSERT(connection_write(client, "test", 4), "write_err");
	ASSERT(connection_close(client), "conn_close");
	release(conn);
	release(client);

	ASSERT_BYTES(0);
}

Test(test_evh_clear) {
	Connection *conn = evh_acceptor((uint8_t[]){127, 0, 0, 1}, 0, 10,
					on_recv2, on_accept2, on_close2);
	uint16_t port = evh_acceptor_port(conn);

	Connection *client =
	    evh_client((uint8_t[]){127, 0, 0, 1}, port, on_recv2, on_close2, 0);

	client->data.inbound.rbuf = alloc(1024);
	client->data.inbound.rbuf_capacity = 1024;
	client->data.inbound.rbuf_offset = 100;

	connection_clear_rbuf_through(client, 200);
	ASSERT_EQ(client->data.inbound.rbuf_offset, 100, "offset=100");
	connection_clear_rbuf_through(client, 50);
	ASSERT_EQ(client->data.inbound.rbuf_offset, 50, "offset=50");
	connection_clear_rbuf(client);
	ASSERT_EQ(client->data.inbound.rbuf_offset, 0, "offset=0");

	ASSERT(!connection_close(client), "conn_close");
	close(conn->socket);
	release(conn);
	release(client);
	release(client->data.inbound.rbuf);

	ASSERT_BYTES(0);
}

int proc_wakeup(int fd);

uint64_t *confirm = NULL;

Test(test_evh_direct) { ASSERT_EQ(proc_wakeup(-1), -1, "proc_wakeup"); }

void ws_on_open(WsConnection *conn) {}
void ws_on_close(WsConnection *conn) {}
int ws_on_message(WsConnection *conn, WsMessage *msg) {
	char buf[1024 * 64];
	memcpy(buf, msg->buffer, msg->len);
	buf[msg->len] = 0;
	ASSERT(!strcmp(buf, "test"), "eqtest");
	__add64(confirm, 1);
	return 0;
}

Test(ws1) {
	confirm = alloc(sizeof(uint64_t));
	*confirm = 0;

	WsConfig config = {
	    .port = 9090, .addr = {0, 0, 0, 0}, .workers = 2, .backlog = 10};
	Ws *ws = init_ws(&config, ws_on_message, ws_on_open, ws_on_close);
	start_ws(ws);

	int socket = socket_connect((uint8_t[]){127, 0, 0, 1}, 9090);
	const char *msg =
	    "GET / HTTP/1.1\r\nSec-WebSocket-Key: "
	    "dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
	char buf[1];
	write(socket, msg, strlen(msg));
	buf[0] = 0x82;
	write(socket, buf, 1);
	buf[0] = 0x04;
	write(socket, buf, 1);
	write(socket, "test", 4);

	while (!ALOAD(confirm)) yield();
	ASSERT(ALOAD(confirm), "confirm");
	stop_ws(ws);
}
