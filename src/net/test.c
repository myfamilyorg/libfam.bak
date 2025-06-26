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

#include <atomic.H>
#include <error.H>
#include <event.H>
#include <evh.H>
#include <socket.H>
#include <sys.H>
#include <syscall_const.H>
#include <test.H>

u8 LOCALHOST[4] = {127, 0, 0, 1};

i32 check_connect(i32 sock) {
	i32 error = 0;
	i32 len = sizeof(error);
	if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		return -1;
	}
	return error;
}

i32 test_connect(const u8 addr[4], u16 port) {
	i32 conn;
	i32 res = socket_connect(&conn, addr, port);
	if (res == 0) {
		return conn;
	}
	if (res == -1 && err != EINPROGRESS) {
		return -1;
	}

	i32 max_attempts = 500;
	i32 attempt = 0;
	while (attempt < max_attempts) {
		i32 error = check_connect(conn);
		if (error == 0) {
			return conn;
		}
		if (error != 0 && error != EINPROGRESS) {
			close(conn);
			return -1;
		}

		sleepm(10);
		attempt++;
	}

	close(conn);
	return -1;
}

Test(socket_connect) {
	u8 buf[10] = {0};
	i32 server = -1, inbound;
	i32 port = socket_listen(&server, LOCALHOST, 0, 10);
	i32 conn;
	err = SUCCESS;

	conn = test_connect(LOCALHOST, port);
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
	i32 fd;
	i32 v;
} ConnectionInfo;

Test(multi_socket) {
	u8 buf[10];
	i32 server, inbound, client, mplex, port, cpid;
	Event events[10];

	mplex = multiplex();
	port = socket_listen(&server, LOCALHOST, 0, 10);
	client = test_connect(LOCALHOST, port);
	mregister(mplex, server, MULTIPLEX_FLAG_ACCEPT, &server);

	if ((cpid = two())) {
		bool do_exit = false;
		while (!do_exit) {
			i32 v, i;
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
					do_exit = true;
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

	waitid(P_PID, cpid, NULL, WEXITED);
}

Test(socket_fails) {
	i32 fd1, fd2, port;
	ASSERT((port = socket_listen(&fd1, LOCALHOST, 0, 1)) > 0, "listen");
	ASSERT(socket_listen(&fd2, LOCALHOST, port, 1) == -1, "listen2");
	close(fd1);
	ASSERT(test_connect(LOCALHOST, port) == -1, "connect");

	_debug_fail_setsockopt = true;
	ASSERT(socket_listen(&fd2, LOCALHOST, 0, 1) == -1, "fail setsockopt");
	_debug_fail_setsockopt = false;

	_debug_fail_listen = true;
	ASSERT(socket_listen(&fd2, LOCALHOST, 0, 1) == -1, "fail setsockopt");
	_debug_fail_listen = false;

	_debug_fail_getsockbyname = true;
	ASSERT(socket_listen(&fd2, LOCALHOST, 0, 1) == -1, "fail setsockopt");
	_debug_fail_getsockbyname = false;

	ASSERT(socket_connect(NULL, LOCALHOST, 1234) == -1, "null input");

	_debug_fail_fcntl = true;
	ASSERT(socket_connect(&fd2, LOCALHOST, 1234) == -1, "fcntl fail");
	_debug_fail_fcntl = false;
}

u64 *evh1_complete = NULL;

i32 evh1_on_accept(void *ctx, Connection *conn) {
	ASSERT_EQ(*((i32 *)ctx), 101, "ctx==101");
	connection_close(conn);
	return 0;
}

i32 evh1_on_recv(void *ctx, Connection *conn, u64 rlen) {
	u8 *rbuf = connection_rbuf(conn);
	u64 offset = connection_rbuf_offset(conn);
	ASSERT_EQ(*((i32 *)ctx), 101, "ctx==101");
	ASSERT_EQ(rlen, 1, "rlen=1");
	ASSERT_EQ(offset, 1, "offset=1");
	ASSERT_EQ(rbuf[0], 'X', "rbuf[0]='X'");
	__add64(evh1_complete, 1);
	connection_clear_rbuf_through(conn, offset);
	connection_clear_rbuf(conn);
	return 0;
}

i32 evh1_on_close(void *ctx, Connection *conn) {
	ASSERT(conn, "conn!=NULL");
	ASSERT_EQ(*((i32 *)ctx), 101, "ctx==101");
	ASSERT_EQ(ALOAD(evh1_complete), 1, "evh1_complete==1");
	__add64(evh1_complete, 1);
	return 0;
}

Test(evh1) {
	i32 ctx = 101, client;
	u16 port = 0;
	Evh *evh1;
	Connection *acceptor;

	evh1_complete = alloc(sizeof(u64));
	ASSERT(evh1_complete, "evh1_complete");
	*evh1_complete = 0;

	acceptor = evh_acceptor(LOCALHOST, port, 10, evh1_on_recv,
				evh1_on_accept, evh1_on_close, 0);
	port = evh_acceptor_port(acceptor);
	evh1 = evh_start(&ctx);
	evh_register(evh1, acceptor);
	client = test_connect(LOCALHOST, port);
	ASSERT(client > 0, "client>0");
	while (write(client, "X", 1) != 1);
	while (ALOAD(evh1_complete) != 2);
	release(evh1_complete);
	evh_stop(evh1);
	connection_release(acceptor);

	ASSERT_BYTES(0);
}

u64 *evh2_complete = NULL;
u64 *evh2_on_connect_val = NULL;

i32 evh2_on_accept(void *ctx, Connection *conn) {
	ASSERT(conn, "conn!=NULL");
	ASSERT_EQ(*((i32 *)ctx), 102, "ctx==102");
	/*connection_close(conn);*/
	return 0;
}

i32 evh2_on_recv(void *ctx, Connection *conn, u64 rlen) {
	u8 *rbuf = connection_rbuf(conn);
	u64 offset = connection_rbuf_offset(conn);
	ASSERT_EQ(*((i32 *)ctx), 102, "ctx==102");
	ASSERT_EQ(rlen, 1, "rlen=1");
	ASSERT_EQ(offset, 1, "offset=1");
	ASSERT_EQ(rbuf[0], 'Z', "rbuf[0]='X'");
	connection_close(conn);

	return 0;
}

i32 evh2_on_close(void *ctx, Connection *conn) {
	ASSERT(conn, "conn!=NULL");
	ASSERT_EQ(*((i32 *)ctx), 102, "ctx==102");
	ASSERT(ALOAD(evh2_complete) <= 2, "evh2_complete <= 2");
	__add64(evh2_complete, 1);
	return 0;
}

i32 evh2_on_connect(void *ctx, Connection *conn, int error) {
	ASSERT(!error, "!error");
	ASSERT_EQ(*((i32 *)ctx), 102, "ctx==102");
	ASSERT(conn, "conn!=NULL");
	__add64(evh2_on_connect_val, 1);
	return 0;
}

Test(evh2) {
	i32 ctx = 102;
	u16 port = 0;
	Evh *evh2;
	Connection *acceptor, *conn;

	evh2_complete = alloc(sizeof(u64));
	ASSERT(evh2_complete, "evh2_complete");
	*evh2_complete = 0;

	evh2_on_connect_val = alloc(sizeof(u64));
	ASSERT(evh2_on_connect_val, "evh2_on_connect_val");
	*evh2_on_connect_val = 0;

	acceptor = evh_acceptor(LOCALHOST, port, 10, evh2_on_recv,
				evh2_on_accept, evh2_on_close, 0);
	port = evh_acceptor_port(acceptor);
	conn = evh_client(LOCALHOST, port, evh2_on_recv, evh2_on_connect,
			  evh2_on_close, 0);
	evh2 = evh_start(&ctx);
	evh_register(evh2, acceptor);
	evh_register(evh2, conn);
	connection_write(conn, "Z", 1);
	while (ALOAD(evh2_complete) < 2);
	ASSERT_EQ(ALOAD(evh2_on_connect_val), 1, "connect success");
	ASSERT_EQ(ALOAD(evh2_complete), 2, "2 closed conns");
	release(evh2_complete);
	release(evh2_on_connect_val);
	evh_stop(evh2);

	connection_release(acceptor);

	ASSERT_BYTES(0);
}

Test(evh3) {
	i32 ctx = 102;
	u16 port = 0;
	Evh *evh2;
	Connection *acceptor, *conn;

	_debug_force_write_buffer = true;

	evh2_complete = alloc(sizeof(u64));
	ASSERT(evh2_complete, "evh2_complete");
	*evh2_complete = 0;

	evh2_on_connect_val = alloc(sizeof(u64));
	ASSERT(evh2_on_connect_val, "evh2_on_connect_val");
	*evh2_on_connect_val = 0;

	acceptor = evh_acceptor(LOCALHOST, port, 10, evh2_on_recv,
				evh2_on_accept, evh2_on_close, 0);
	port = evh_acceptor_port(acceptor);
	conn = evh_client(LOCALHOST, port, evh2_on_recv, evh2_on_connect,
			  evh2_on_close, 0);
	evh2 = evh_start(&ctx);
	evh_register(evh2, acceptor);
	evh_register(evh2, conn);
	connection_write(conn, "Z", 1);
	while (ALOAD(evh2_complete) < 2);
	ASSERT_EQ(ALOAD(evh2_on_connect_val), 1, "connect success");
	ASSERT_EQ(ALOAD(evh2_complete), 2, "2 closed conns");
	release(evh2_complete);
	release(evh2_on_connect_val);
	evh_stop(evh2);

	connection_release(acceptor);

	ASSERT_BYTES(0);

	_debug_force_write_buffer = false;
}

u8 BAD_ADDR[4] = {255, 255, 255, 255};

Test(connection_err) {
	u16 port;
	Connection *c2;
	Connection *c1 = evh_acceptor(LOCALHOST, 0, 1, evh1_on_recv,
				      evh1_on_accept, evh1_on_close, 0);
	ASSERT(c1, "c1");
	port = evh_acceptor_port(c1);
	ASSERT(!evh_acceptor(LOCALHOST, port, 1, evh1_on_recv, evh1_on_accept,
			     evh1_on_close, 0),
	       "port used");

	c2 = evh_client(LOCALHOST, port, evh1_on_recv, evh2_on_connect,
			evh1_on_close, 0);
	ASSERT(!evh_acceptor_port(c2), "not acceptor");
	ASSERT(connection_set_mplex(c1, 0), "set mplex acceptor err");
	ASSERT(connection_write(c1, "x", 1), "write to acceptor");
	ASSERT(connection_write_complete(c1), "write complete");
	ASSERT(!connection_on_accept(c2),
	       "connection_on_accept for non-acceptor");
	ASSERT(!connection_on_connect(c1),
	       "connection_on_connect for non-outbound");

	connection_release(c1);
	connection_release(c2);

	ASSERT(!evh_client(BAD_ADDR, port, evh1_on_recv, evh2_on_connect,
			   evh1_on_close, 0),
	       "bad addr");

	c1 = evh_acceptor(LOCALHOST, 0, 1, evh1_on_recv, evh1_on_accept,
			  evh1_on_close, 0);

	connection_close(c1);
	connection_release(c1);

	ASSERT_BYTES(0);
}
