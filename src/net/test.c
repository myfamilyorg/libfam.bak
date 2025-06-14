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
#include <socket.H>
#include <syscall_const.H>
#include <test.H>
#include <ws.H>

uint8_t LOCALHOST[4] = {127, 0, 0, 1};

Test(event) {
	Event events[10];
	int val = 101;
	int fds[2];
	int mplex = multiplex();
	int x;
	ASSERT(mplex > 0, "mplex");
	pipe(fds);
	ASSERT_EQ(mregister(mplex, fds[0], MULTIPLEX_FLAG_READ, &val), 0,
		  "mreg");
	ASSERT_EQ(write(fds[1], "abc", 3), 3, "write");
	err = 0;
	x = mwait(mplex, events, 10, 10);
	ASSERT_EQ(x, 1, "mwait");

	close(mplex);
	close(fds[0]);
	close(fds[1]);
}

Test(socket_connect) {
	char buf[10] = {0};
	int server = -1, inbound;
	int port = socket_listen(&server, LOCALHOST, 0, 10);
	int conn = socket_connect(LOCALHOST, port);

	write(conn, "test", 4);
	inbound = socket_accept(server);
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
	port = socket_listen(&server, LOCALHOST, 0, 10);
	client = socket_connect(LOCALHOST, port);
	mregister(mplex, server, MULTIPLEX_FLAG_ACCEPT, &server);

	if (two()) {
		bool exit = false;
		while (!exit) {
			int v, i;
			err = 0;
			v = mwait(mplex, events, 10, -1);
			for (i = 0; i < v; i++) {
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

Test(socket_fails) {
	int fd1, fd2, port;
	ASSERT((port = socket_listen(&fd1, LOCALHOST, 0, 1)) > 0, "listen");
	ASSERT(socket_listen(&fd2, LOCALHOST, port, 1) == -1, "listen2");
	close(fd1);
	ASSERT(socket_connect(LOCALHOST, port) == -1, "connect");
}

uint64_t *value;
Evh evh1;

int on_accept(void *ctx __attribute__((unused)),
	      Connection *conn __attribute__((unused))) {
	__add64(value, 1);
	return 0;
}

int on_recv(void *ctx __attribute__((unused)), Connection *conn,
	    size_t rlen __attribute__((unused))) {
	char buf[1024 * 64];
	InboundData *ib = &conn->data.inbound;
	memcpy(buf, ib->rbuf, ib->rbuf_offset);
	buf[ib->rbuf_offset] = 0;
	connection_write(conn, "0123456789\n", 11);
	connection_close(conn);
	return 0;
}

int on_close(void *ctx __attribute__((unused)),
	     Connection *conn __attribute__((unused))) {
	return 0;
}

Test(test_evh1) {
	int port, tconn, total, x;
	char buf[100];
	Connection *conn;
	ASSERT_BYTES(0);
	value = alloc(sizeof(uint64_t));
	*value = 0;

	ASSERT(!evh_start(&evh1, NULL, 0), "evh_start");
	conn = evh_acceptor(LOCALHOST, 0, 10, on_recv, on_accept, on_close);
	port = evh_acceptor_port(conn);

	evh_register(&evh1, conn);

	tconn = socket_connect(LOCALHOST, port);
	write(tconn, "test1\n", 6);

	total = 0;
	while (total < 11) {
		err = 0;
		x = read(tconn, buf + total, 100 - total);
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

int on_accept2(void *ctx __attribute__((unused)),
	       Connection *conn __attribute__((unused))) {
	return 0;
}

int on_recv2(void *ctx __attribute__((unused)), Connection *conn,
	     size_t rlen __attribute__((unused))) {
	InboundData *ib = &conn->data.inbound;
	int *v = (int *)(ib->rbuf + ib->rbuf_offset - rlen);
	int nv = *v + 1;

	ASSERT_EQ(rlen, sizeof(int), "sizeof(int)");
	ASTORE(value2, nv);

	if (nv < 105)
		connection_write(conn, (uint8_t *)&nv, sizeof(int));
	else
		connection_close(conn);
	return 0;
}

int on_close2(void *ctx __attribute__((unused)),
	      Connection *conn __attribute__((unused))) {
	return 0;
}

Test(test_evh2) {
	int port, initial = 101;
	Connection *conn, *client;
	ASSERT_BYTES(0);

	value2 = alloc(sizeof(int));
	*value2 = 0;
	ASSERT(!evh_start(&evh2, NULL, 0), "evh_start");

	conn = evh_acceptor(LOCALHOST, 0, 10, on_recv2, on_accept2, on_close2);
	port = evh_acceptor_port(conn);
	evh_register(&evh2, conn);

	client = evh_client(LOCALHOST, port, on_recv2, on_close2, 0);
	ASSERT(!evh_acceptor_port(client), "evh_acceptor_port on client err");
	ASSERT(client->socket > 0, "connect");
	evh_register(&evh2, client);

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
	Connection *conn =
	    evh_acceptor(LOCALHOST, 0, 10, on_recv2, on_accept2, on_close2);
	uint16_t port = evh_acceptor_port(conn);
	ASSERT(
	    !evh_acceptor(LOCALHOST, port, 10, on_recv2, on_accept2, on_close2),
	    "listen on same port");
	close(conn->socket);
	release(conn);
	ASSERT(!evh_client(LOCALHOST, port, on_recv2, on_close2, 0),
	       "connect failure");

	ASSERT_BYTES(0);
}

Test(test_evh_other_situations) {
	Connection *conn =
	    evh_acceptor(LOCALHOST, 0, 10, on_recv2, on_accept2, on_close2);
	uint16_t port = evh_acceptor_port(conn);

	Connection *client =
	    evh_client(LOCALHOST, port, on_recv2, on_close2, 0);

	ASSERT(!connection_close(client), "conn_close");

	close(conn->socket);
	release(conn);
	release(client);

	ASSERT_BYTES(0);
}

Test(test_evh_other_situations2) {
	Connection *conn =
	    evh_acceptor(LOCALHOST, 0, 10, on_recv2, on_accept2, on_close2);
	uint16_t port = evh_acceptor_port(conn);

	Connection *client =
	    evh_client(LOCALHOST, port, on_recv2, on_close2, 0);
	ASSERT(client, "client");

	close(conn->socket);
	ASSERT(connection_write(client, "test", 4), "write_err");
	ASSERT(connection_close(client), "conn_close");
	release(conn);
	release(client);

	ASSERT_BYTES(0);
}

Test(test_evh_clear) {
	Connection *conn =
	    evh_acceptor(LOCALHOST, 0, 10, on_recv2, on_accept2, on_close2);
	uint16_t port = evh_acceptor_port(conn);

	Connection *client =
	    evh_client(LOCALHOST, port, on_recv2, on_close2, 0);

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

void ws_on_open(WsConnection *conn __attribute__((unused))) {}
void ws_on_close(WsConnection *conn __attribute__((unused))) {}
int ws_on_message(WsConnection *conn, WsMessage *msg) {
	char buf[1024 * 64];

	memcpy(buf, msg->buffer, msg->len);
	buf[msg->len] = 0;
	ASSERT(!strcmp(buf, "test"), "eqtest");
	__add64(confirm, 1);
	ws_connection_close(conn, 0, "test");
	return 0;
}

/* Pick port, localhost, backlog = 10, 2 workers */
WsConfig WS_STD_CFG = {0, {127, 0, 0, 1}, 10, 2};
/* Port 3737 on INADDR_ANY, backlog = 10, 2 workers */
WsConfig WS_EXT = {3737, {0, 0, 0, 0}, 10, 2};

Test(ws1) {
	uint16_t port;
	Ws *ws;
	uint8_t buf[1];
	int socket;
	const char *msg =
	    "GET / HTTP/1.1\r\nSec-WebSocket-Key: "
	    "dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";

	ASSERT_BYTES(0);
	confirm = alloc(sizeof(uint64_t));
	*confirm = 0;
	ws = ws_init(&WS_STD_CFG, ws_on_message, ws_on_open, ws_on_close);
	ws_start(ws);
	port = ws_port(ws);

	socket = socket_connect(LOCALHOST, port);
	write(socket, msg, strlen(msg));
	buf[0] = 0x82;
	write(socket, buf, 1);
	buf[0] = 0x04;
	write(socket, buf, 1);
	write(socket, "test", 4);

	while (!ALOAD(confirm)) yield();
	ASSERT_EQ(ALOAD(confirm), 1, "confirm");

	ws_stop(ws);
	close(socket);
	release(confirm);

	ASSERT_BYTES(0);
}

void ws_on_open2(WsConnection *conn __attribute__((unused))) {}
void ws_on_close2(WsConnection *conn __attribute__((unused))) {}
int ws_on_message2(WsConnection *conn, WsMessage *msg) {
	char *buf = alloc(msg->len + 1);
	memcpy(buf, msg->buffer, msg->len);
	buf[msg->len] = 0;
	ws_send(conn, msg);
	release(buf);
	return 0;
}

Test(ws2) {
	Ws *ws = ws_init(&WS_EXT, ws_on_message2, ws_on_open2, ws_on_close2);
	ws_start(ws);
	/* sleepm(10000000); */
	ws_stop(ws);
	ASSERT_BYTES(0);
}

