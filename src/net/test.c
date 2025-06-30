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
#include <connection_internal.H>
#include <error.H>
#include <event.H>
/*#include <evh.H>*/

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

	ASSERT(set_nonblocking(0) == -1, "set nonblocking on 0");
	ASSERT(socket_listen(0, LOCALHOST, 0, 1) == -1, "0 multiplex err");
	ASSERT(socket_accept(0) == -1, "accept on 0");

	port = socket_listen(&fd1, LOCALHOST, 0, 1);
	fd2 = test_connect(LOCALHOST, port);
	_debug_setnonblocking_err = true;
	ASSERT(socket_accept(fd1) == -1, "accept with nonblock err");
	ASSERT(socket_listen((void *)1, LOCALHOST, 0, 1) == -1,
	       "listen with nonblock err");
	_debug_setnonblocking_err = false;
	close(fd1);
	close(fd2);

	ASSERT_EQ(mregister(-1, -1, 0, NULL), -1, "mreg invalid");
	ASSERT_EQ(mwait(-1, NULL, 0, -1), -1, "mwait invalid");
}

void c1_on_accept(void *ctx __attribute__((unused)),
		  Connection *conn __attribute__((unused))) {}

void c1_on_recv(void *ctx __attribute__((unused)),
		Connection *conn __attribute__((unused)),
		u64 rlen __attribute__((unused))) {}

void c1_on_close(void *ctx __attribute__((unused)),
		 Connection *conn __attribute__((unused))) {}

void c1_on_connect(void *ctx __attribute__((unused)),
		   Connection *conn __attribute__((unused)),
		   int err __attribute__((unused))) {}

u8 BAD_ADDR[4] = {255, 255, 255, 255};

Test(connection1) {
	u16 port;
	Connection *c2;
	Connection *c1 = evh_acceptor(LOCALHOST, 0, 1, c1_on_recv, c1_on_accept,
				      c1_on_close, 0);
	ASSERT(c1, "c1");
	port = evh_acceptor_port(c1);
	ASSERT(!evh_acceptor(LOCALHOST, port, 1, c1_on_recv, c1_on_accept,
			     c1_on_close, 0),
	       "port used");

	c2 = evh_client(LOCALHOST, port, c1_on_recv, c1_on_connect, c1_on_close,
			0);
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

	ASSERT(!evh_client(BAD_ADDR, port, c1_on_recv, c1_on_connect,
			   c1_on_close, 0),
	       "bad addr");

	c1 = evh_acceptor(LOCALHOST, 0, 1, c1_on_recv, c1_on_accept,
			  c1_on_close, 0);

	connection_close(c1);
	connection_release(c1);

	ASSERT_BYTES(0);
}

Test(connection2) {
	u8 buf[64] = {0};
	i32 fd;
	Connection *c1 = evh_acceptor(LOCALHOST, 0, 1, c1_on_recv, c1_on_accept,
				      c1_on_close, 0);
	u16 port = evh_acceptor_port(c1);
	Connection *c2 = evh_client(LOCALHOST, port, c1_on_recv, c1_on_connect,
				    c1_on_close, 0);
	Connection *c3;
	i32 mplex = multiplex();

	while ((fd = socket_accept(connection_socket(c1))) == -1);
	c3 = evh_accepted(fd, c1_on_recv, c1_on_close, 0);
	ASSERT(c3, "c3!=NULL");

	ASSERT_EQ(connection_type(c1), Acceptor, "Acceptor");
	ASSERT_EQ(connection_type(c2), Outbound, "Outbound");
	ASSERT_EQ(connection_type(c3), Inbound, "Inbound");

	ASSERT(!connection_write(c3, "1", 1), "connection_write1");
	while (read(connection_socket(c2), buf, sizeof(buf)) != 1) {
	}
	ASSERT_EQ(buf[0], '1', "1");

	_debug_force_write_buffer = true;
	err = 0;
	connection_set_mplex(c3, mplex);
	ASSERT(!connection_write(c3, "2", 1), "connection_write2");
	_debug_force_write_buffer = false;
	connection_write_complete(c3);
	while (read(connection_socket(c2), buf, sizeof(buf)) != 1) {
	}
	ASSERT_EQ(buf[0], '2', "2");

	_debug_force_write_buffer = true;
	connection_set_mplex(c3, -1);
	ASSERT(connection_write(c3, "3", 1), "connection_write3");
	connection_set_mplex(c3, mplex);
	/* Connection now closed */
	ASSERT(connection_write(c3, "4", 1), "connection_write4");

	_debug_force_write_buffer = false;

	/* Since we're not using rbuf (like evh), it's uninitialized */
	ASSERT(!connection_rbuf(c3), "rbuf==NULL");
	ASSERT(!connection_rbuf_offset(c3), "rbuf_offset=0");
	ASSERT(!connection_rbuf_capacity(c3), "rbuf_capacity=0");
	connection_clear_rbuf_through(c3, 0);
	connection_clear_rbuf(c3);
	ASSERT(!connection_set_rbuf_offset(c3, 0), "set_rbuf");
	ASSERT(!connection_is_connected(c2), "c2 not conn");
	connection_set_is_connected(c2);
	ASSERT(connection_is_connected(c2), "c2 conn");
	ASSERT(!connection_alloc_overhead(c1), "no overhead");
	ASSERT(connection_on_recv(c1), "c1 on_recv != NULL");
	ASSERT(connection_on_recv(c2), "c2 on_recv != NULL");
	ASSERT(connection_on_close(c1), "c1 on_close != NULL");
	ASSERT(connection_on_close(c2), "c2 on_close != NULL");
	ASSERT(connection_on_accept(c1), "c1 on_accept != NULL");
	ASSERT(connection_on_connect(c2), "c2 on_connect != NULL");

	ASSERT(!connection_check_capacity(c2), "c2 can add capacity");
	ASSERT(connection_check_capacity(c1), "c1 can not add capacity");

	close(mplex);
	close(fd);
	connection_close(c3);

	err = 0;
	ASSERT(connection_write(c3, "x", 1), "connection_writex");
	ASSERT(err, EIO); /* Already closed */

	connection_close(c2);
	connection_close(c1);
	connection_release(c3);
	connection_release(c2);
	connection_release(c1);

	ASSERT_BYTES(0);
}

Test(connection3) {
	u8 buf[64] = {0};
	i32 fd;
	Connection *c1 = evh_acceptor(LOCALHOST, 0, 1, c1_on_recv, c1_on_accept,
				      c1_on_close, 0);
	u16 port = evh_acceptor_port(c1);
	Connection *c2 = evh_client(LOCALHOST, port, c1_on_recv, c1_on_connect,
				    c1_on_close, 0);
	Connection *c3;
	i32 mplex = multiplex();

	while ((fd = socket_accept(connection_socket(c1))) == -1);
	c3 = evh_accepted(fd, c1_on_recv, c1_on_close, 0);
	ASSERT(c3, "c3!=NULL");

	ASSERT_EQ(connection_type(c1), Acceptor, "Acceptor");
	ASSERT_EQ(connection_type(c2), Outbound, "Outbound");
	ASSERT_EQ(connection_type(c3), Inbound, "Inbound");

	ASSERT(!connection_write(c3, "1", 1), "connection_write1");
	while (read(connection_socket(c2), buf, sizeof(buf)) != 1) {
	}
	ASSERT_EQ(buf[0], '1', "1");

	_debug_force_write_buffer = true;
	err = 0;
	connection_set_mplex(c3, mplex);
	ASSERT(!connection_write(c3, "2", 1), "connection_write2");
	_debug_force_write_buffer = false;
	connection_set_mplex(c3, -1);
	connection_write_complete(c3);
	while (read(connection_socket(c2), buf, sizeof(buf)) != 1) {
	}
	ASSERT_EQ(buf[0], '2', "2");

	close(mplex);
	connection_close(c2);
	connection_close(c1);
	connection_release(c3);
	connection_release(c2);
	connection_release(c1);

	ASSERT_BYTES(0);
}

Test(connection4) {
	u8 buf[64] = {0};
	i32 fd;
	Connection *c1 = evh_acceptor(LOCALHOST, 0, 1, c1_on_recv, c1_on_accept,
				      c1_on_close, 0);
	u16 port = evh_acceptor_port(c1);
	Connection *c2 = evh_client(LOCALHOST, port, c1_on_recv, c1_on_connect,
				    c1_on_close, 0);
	Connection *c3;
	i32 mplex = multiplex();

	while ((fd = socket_accept(connection_socket(c1))) == -1);
	c3 = evh_accepted(fd, c1_on_recv, c1_on_close, 0);
	ASSERT(c3, "c3!=NULL");

	ASSERT_EQ(connection_type(c1), Acceptor, "Acceptor");
	ASSERT_EQ(connection_type(c2), Outbound, "Outbound");
	ASSERT_EQ(connection_type(c3), Inbound, "Inbound");

	ASSERT(!connection_write(c3, "1", 1), "connection_write1");
	while (read(connection_socket(c2), buf, sizeof(buf)) != 1) {
	}
	ASSERT_EQ(buf[0], '1', "1");

	connection_set_mplex(c3, mplex);
	_debug_alloc_failure = 1;
	_debug_force_write_buffer = true;
	ASSERT(connection_write(c3, "y", 1), "connection_writey");
	_debug_force_write_buffer = false;

	close(mplex);
	connection_close(c2);
	connection_close(c1);
	connection_release(c3);
	connection_release(c2);
	connection_release(c1);

	ASSERT_BYTES(0);
}

Test(connection5) {
	u8 buf[64] = {0};
	i32 fd;
	Connection *c1 = evh_acceptor(LOCALHOST, 0, 1, c1_on_recv, c1_on_accept,
				      c1_on_close, 0);
	u16 port = evh_acceptor_port(c1);
	Connection *c2 = evh_client(LOCALHOST, port, c1_on_recv, c1_on_connect,
				    c1_on_close, 0);
	Connection *c3;
	i32 mplex = multiplex();

	while ((fd = socket_accept(connection_socket(c1))) == -1);
	c3 = evh_accepted(fd, c1_on_recv, c1_on_close, 0);
	ASSERT(c3, "c3!=NULL");

	ASSERT_EQ(connection_type(c1), Acceptor, "Acceptor");
	ASSERT_EQ(connection_type(c2), Outbound, "Outbound");
	ASSERT_EQ(connection_type(c3), Inbound, "Inbound");

	ASSERT(!connection_write(c3, "1", 1), "connection_write1");
	while (read(connection_socket(c2), buf, sizeof(buf)) != 1) {
	}
	ASSERT_EQ(buf[0], '1', "1");

	connection_set_mplex(c3, mplex);
	_debug_force_write_error = true;
	ASSERT(connection_write(c3, "y", 1), "connection_writey");
	_debug_force_write_error = false;

	close(mplex);
	connection_close(c2);
	connection_close(c1);
	connection_release(c3);
	connection_release(c2);
	connection_release(c1);

	ASSERT_BYTES(0);
}

Test(connection6) {
	u8 buf[64] = {0};
	i32 fd;
	Connection *c1 = evh_acceptor(LOCALHOST, 0, 1, c1_on_recv, c1_on_accept,
				      c1_on_close, 0);
	u16 port = evh_acceptor_port(c1);
	Connection *c2 = evh_client(LOCALHOST, port, c1_on_recv, c1_on_connect,
				    c1_on_close, 0);
	Connection *c3;
	i32 mplex = multiplex();

	while ((fd = socket_accept(connection_socket(c1))) == -1);
	c3 = evh_accepted(fd, c1_on_recv, c1_on_close, 0);
	ASSERT(c3, "c3!=NULL");

	ASSERT_EQ(connection_type(c1), Acceptor, "Acceptor");
	ASSERT_EQ(connection_type(c2), Outbound, "Outbound");
	ASSERT_EQ(connection_type(c3), Inbound, "Inbound");

	ASSERT(!connection_write(c3, "1", 1), "connection_write1");
	while (read(connection_socket(c2), buf, sizeof(buf)) != 1) {
	}
	ASSERT_EQ(buf[0], '1', "1");

	connection_set_mplex(c3, mplex);
	_debug_force_write_error = true;
	_debug_write_error_code = EINTR;
	/* Interrupt should retry */
	ASSERT(!connection_write(c3, "y", 1), "connection_writey");
	_debug_write_error_code = EIO;
	_debug_force_write_error = false;

	while (read(connection_socket(c2), buf, sizeof(buf)) != 1) {
	}
	ASSERT_EQ(buf[0], 'y', "y");

	close(mplex);
	connection_close(c2);
	connection_close(c1);
	connection_release(c3);
	connection_release(c2);
	connection_release(c1);

	ASSERT_BYTES(0);
}

Test(connection7) {
	i32 fd;
	Connection *c1 = evh_acceptor(LOCALHOST, 0, 1, c1_on_recv, c1_on_accept,
				      c1_on_close, 0);
	u16 port = evh_acceptor_port(c1);
	Connection *c2 = evh_client(LOCALHOST, port, c1_on_recv, c1_on_connect,
				    c1_on_close, 0);
	Connection *c3;
	i32 mplex = multiplex();

	while ((fd = socket_accept(connection_socket(c1))) == -1);
	c3 = evh_accepted(fd, c1_on_recv, c1_on_close, 0);
	ASSERT(c3, "c3!=NULL");

	ASSERT_EQ(connection_type(c1), Acceptor, "Acceptor");
	ASSERT_EQ(connection_type(c2), Outbound, "Outbound");
	ASSERT_EQ(connection_type(c3), Inbound, "Inbound");

	_debug_force_write_buffer = true;
	err = 0;
	connection_set_mplex(c3, mplex);
	ASSERT(!connection_write(c3, "1", 1), "connection_write1");
	_debug_force_write_buffer = false;
	connection_set_mplex(c3, -1);
	_debug_force_write_error = true;
	ASSERT_EQ(connection_write_complete(c3), -1, "write complete err");
	_debug_force_write_error = false;

	close(mplex);
	connection_close(c2);
	connection_close(c1);
	connection_release(c3);
	connection_release(c2);
	connection_release(c1);

	ASSERT_BYTES(0);
}
