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
#include <evh.H>
#include <socket.H>
#include <sys.H>
#include <syscall.H>
#include <syscall_const.H>
#include <test.H>
#include <vec.H>
#include <ws.H>

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

		sleep(10);
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

	shutdown(inbound, SHUT_RD);
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

u8 BAD_ADDR[4] = {255, 255, 255, 255};

Test(conn1) {
	Connection *c1 = connection_acceptor(LOCALHOST, 0, 10, 0);
	Connection *c2, *c3;
	u8 buf[2] = {0};
	i32 fd, mplex;
	u16 port;
	ASSERT(c1, "c1!=NULL");

	mplex = multiplex();

	port = connection_acceptor_port(c1);
	ASSERT(!connection_acceptor(LOCALHOST, port, 10, 0), "port conflict");
	c2 = connection_client(LOCALHOST, port, 0);
	connection_set_mplex(c2, mplex);
	ASSERT(c2, "c2!=NULL");
	ASSERT_EQ(connection_acceptor_port(c2), -1, "not acceptor");
	ASSERT(!connection_client(BAD_ADDR, port, 0), "bad addr");
	while ((fd = accept(connection_socket(c1), NULL, NULL)) < 0);
	c3 = connection_accepted(fd, mplex, 0);
	ASSERT(c3, "c3!=NULL");

	ASSERT(!connection_write(c3, "x", 1), "write success");

	while (read(connection_socket(c2), buf, 2) != 1);
	ASSERT_EQ(buf[0], 'x', "read x");

	_debug_force_write_buffer = true;
	ASSERT(!connection_write(c2, "y", 1), "write success2");
	_debug_force_write_buffer = false;
	ASSERT(!connection_write_complete(c2), "write complete");
	while (read(connection_socket(c3), buf, 2) != 1);
	ASSERT_EQ(buf[0], 'y', "read y");

	ASSERT(connection_set_mplex(c1, 0), "set mplex on acceptor");
	ASSERT(connection_write(c1, "test", 4), "write acceptor");

	_debug_force_write_error = true;
	_debug_write_error_code = EINTR;
	ASSERT(!connection_write(c2, "z", 1), "write success3");
	_debug_force_write_error = false;
	_debug_write_error_code = SUCCESS;
	while (read(connection_socket(c3), buf, 2) != 1);
	ASSERT_EQ(buf[0], 'z', "read z");

	_debug_force_write_buffer = true;
	ASSERT(!connection_write(c3, "a", 1), "write successa");
	_debug_force_write_buffer = false;
	_debug_force_write_error = true;
	_debug_write_error_code = EINTR;
	ASSERT(connection_write(c3, "c", U64_MAX), "overflow");
	ASSERT(!connection_write_complete(c3), "write completea");
	while (read(connection_socket(c2), buf, 2) != 1);
	ASSERT_EQ(buf[0], 'a', "read a");

	_debug_force_write_error = true;
	_debug_write_error_code = EPIPE;
	ASSERT(connection_write(c2, "z", 1), "write fail");
	_debug_force_write_error = false;
	_debug_write_error_code = SUCCESS;

	ASSERT(connection_write(c2, "test", 4), "write closed");

	ASSERT(connection_write_complete(c1), "write complete on acceptor");
	ASSERT(connection_write_complete(c2), "write complete on closed");

	close(connection_socket(c2));
	close(connection_socket(c3));

	connection_release(c1);
	connection_release(c2);
	connection_release(c3);
	close(mplex);

	ASSERT_BYTES(0);
}

Test(conn2) {
	Connection *c1 = connection_acceptor(LOCALHOST, 0, 10, 16);
	Connection *c2, *c3;
	i32 fd, mplex;
	u16 port;
	ASSERT(c1, "c1!=NULL");

	mplex = multiplex();

	port = connection_acceptor_port(c1);
	ASSERT(!connection_acceptor(LOCALHOST, port, 10, 0), "port conflict");
	c2 = connection_client(LOCALHOST, port, 0);
	connection_set_mplex(c2, mplex);
	ASSERT(c2, "c2!=NULL");
	ASSERT_EQ(connection_acceptor_port(c2), -1, "not acceptor");
	ASSERT(!connection_client(BAD_ADDR, port, 0), "bad addr");
	while ((fd = accept(connection_socket(c1), NULL, NULL)) < 0);
	c3 = connection_accepted(fd, mplex, 0);
	ASSERT(c3, "c3!=NULL");

	ASSERT(!connection_rbuf(c2), "rbuf c2==NULL");
	ASSERT(!connection_rbuf(c1), "rbuf c1==NULL");
	ASSERT(!connection_wbuf(c2), "wbuf c2==NULL");
	ASSERT(!connection_wbuf(c1), "wbuf c1==NULL");

	ASSERT_EQ(connection_alloc_overhead(c1), 16, "c1 alloc overhead == 16");
	ASSERT_EQ(connection_alloc_overhead(c2), -1, "c2 alloc overhead err");
	ASSERT(!connection_is_connected(c3), "c3 not conn");
	connection_set_is_connected(c3);
	connection_set_is_connected(c1);
	ASSERT(connection_is_connected(c3), "c3 conn");
	ASSERT(connection_size() <= 64, "size fits in cache line");

	_debug_force_write_buffer = true;
	ASSERT(!connection_write(c2, "a", 1), "write successa");
	_debug_force_write_buffer = false;
	_debug_force_write_error = true;
	_debug_write_error_code = EPIPE;
	ASSERT(connection_write_complete(c2), "write completea fail");
	_debug_force_write_error = false;

	ASSERT(!connection_close(c1), "close c1");
	ASSERT(connection_close(c1), "double close c1");

	ASSERT(!connection_close(c3), "close c3");
	ASSERT(connection_close(c3), "double close c3");

	close(connection_socket(c2));
	close(connection_socket(c3));

	connection_release(c1);
	connection_release(c2);
	connection_release(c3);
	close(mplex);

	ASSERT_BYTES(0);
}

Test(conn3) {
	Connection *c1 = connection_acceptor(LOCALHOST, 0, 10, 16);
	Connection *c2, *c3;
	i32 fd, mplex;
	u16 port;
	ASSERT(c1, "c1!=NULL");

	mplex = multiplex();

	port = connection_acceptor_port(c1);
	ASSERT(!connection_acceptor(LOCALHOST, port, 10, 0), "port conflict");
	c2 = connection_client(LOCALHOST, port, 0);
	connection_set_mplex(c2, mplex);
	ASSERT(c2, "c2!=NULL");
	ASSERT_EQ(connection_acceptor_port(c2), -1, "not acceptor");
	while ((fd = accept(connection_socket(c1), NULL, NULL)) < 0);
	c3 = connection_accepted(fd, -1, 0);
	ASSERT(c3, "c3!=NULL");

	_debug_force_write_buffer = true;
	ASSERT(connection_write(c3, "a", 1), "invalid mplex");

	_debug_alloc_failure = 1;
	ASSERT(connection_write(c2, "a", 1), "alloc failure");

	_debug_force_write_buffer = false;

	close(connection_socket(c2));
	close(connection_socket(c3));

	connection_release(c1);
	connection_release(c2);
	connection_release(c3);
	close(mplex);

	ASSERT_BYTES(0);
}

Test(conn4) {
	Connection *c1 = connection_acceptor(LOCALHOST, 0, 10, 16);
	Connection *c2, *c3;
	i32 fd, mplex;
	u16 port;
	ASSERT(c1, "c1!=NULL");

	mplex = multiplex();

	port = connection_acceptor_port(c1);
	ASSERT(!connection_acceptor(LOCALHOST, port, 10, 0), "port conflict");
	c2 = connection_client(LOCALHOST, port, 0);
	connection_set_mplex(c2, mplex);
	ASSERT(c2, "c2!=NULL");
	ASSERT_EQ(connection_acceptor_port(c2), -1, "not acceptor");
	while ((fd = accept(connection_socket(c1), NULL, NULL)) < 0);
	c3 = connection_accepted(fd, mplex, 0);
	ASSERT(c3, "c3!=NULL");

	_debug_force_write_buffer = true;
	ASSERT(!connection_write(c3, "a", 1), "write with force to buffer");

	connection_set_mplex(c3, -1);
	ASSERT(connection_write_complete(c3), "write complete fail");

	_debug_force_write_buffer = false;

	close(connection_socket(c2));
	close(connection_socket(c3));

	connection_release(c1);
	connection_release(c2);
	connection_release(c3);
	close(mplex);

	ASSERT_BYTES(0);
}

Test(conn5) {
	Connection *c1 = connection_acceptor(LOCALHOST, 0, 10, 16);
	Connection *c2, *c3;
	i32 fd, mplex;
	u8 buf[10];
	u16 port;
	Vec *v;
	ASSERT(c1, "c1!=NULL");

	mplex = multiplex();

	port = connection_acceptor_port(c1);
	ASSERT(!connection_acceptor(LOCALHOST, port, 10, 0), "port conflict");
	c2 = connection_client(LOCALHOST, port, 0);
	connection_set_mplex(c2, mplex);
	ASSERT(c2, "c2!=NULL");
	ASSERT_EQ(connection_acceptor_port(c2), -1, "not acceptor");
	while ((fd = accept(connection_socket(c1), NULL, NULL)) < 0);
	c3 = connection_accepted(fd, mplex, 0);
	ASSERT(c3, "c3!=NULL");

	_debug_force_write_buffer = true;
	ASSERT(!connection_write(c3, "ab", 2), "write with force to buffer");

	_debug_connection_wmax = 1;
	ASSERT(!connection_write_complete(c3), "write complete 1");
	_debug_connection_wmax = 0;

	while (read(connection_socket(c2), buf, 2) != 1);
	ASSERT_EQ(buf[0], 'a', "a");

	ASSERT(!connection_write_complete(c3), "write complete 2");

	while (read(connection_socket(c2), buf, 2) != 1);
	ASSERT_EQ(buf[0], 'b', "b");

	_debug_force_write_buffer = false;

	v = connection_rbuf(c2);
	ASSERT(!v, "connection_rbuf1");
	v = vec_resize(v, 100);
	connection_set_rbuf(c2, v);
	v = connection_rbuf(c2);
	ASSERT(v, "connection_rbuf2");

	close(connection_socket(c2));
	close(connection_socket(c3));

	connection_release(c1);
	connection_release(c2);
	connection_release(c3);
	close(mplex);

	ASSERT_BYTES(0);
}

u64 *evh1_complete = NULL;
u64 *evh1_on_connect_val = NULL;

void evh1_on_accept(void *ctx, Connection *conn) {
	ASSERT(conn, "conn!=NULL");
	ASSERT_EQ(*((i32 *)ctx), 102, "ctx==102");
}

void evh1_on_recv(void *ctx, Connection *conn, u64 rlen) {
	Vec *rbuf = connection_rbuf(conn);
	u64 offset = vec_elements(rbuf);
	u8 *data = vec_data(rbuf);
	ASSERT_EQ(*((i32 *)ctx), 102, "ctx==102");
	ASSERT_EQ(rlen, 1, "rlen=1");
	ASSERT_EQ(offset, 1, "offset=1");
	ASSERT_EQ(data[0], 'Z', "data[0]='Z'");
	connection_close(conn);
}

void evh1_on_close(void *ctx, Connection *conn) {
	ASSERT(conn, "conn!=NULL");
	ASSERT_EQ(*((i32 *)ctx), 102, "ctx==102");
	ASSERT(ALOAD(evh1_complete) <= 2, "evh1_complete <= 2");
	__add64(evh1_complete, 1);
}

void evh1_on_connect(void *ctx, Connection *conn,
		     int error __attribute__((unused))) {
	ASSERT_EQ(*((i32 *)ctx), 102, "ctx==102");
	ASSERT(conn, "conn!=NULL");
	__add64(evh1_on_connect_val, 1);
}

Test(evh1) {
	i32 ctx = 102;
	u16 port = 0;
	Evh *evh1;
	Connection *acceptor, *conn;
	EvhConfig config = {&ctx, evh1_on_recv, evh1_on_accept, evh1_on_connect,
			    evh1_on_close};

	evh1_complete = alloc(sizeof(u64));
	ASSERT(evh1_complete, "evh1_complete");
	*evh1_complete = 0;

	evh1_on_connect_val = alloc(sizeof(u64));
	ASSERT(evh1_on_connect_val, "evh1_on_connect_val");
	*evh1_on_connect_val = 0;

	acceptor = connection_acceptor(LOCALHOST, port, 10, 0);
	port = connection_acceptor_port(acceptor);
	conn = connection_client(LOCALHOST, port, 0);
	ASSERT(!connection_is_connected(conn), "not connected");
	evh1 = evh_init(&config);
	ASSERT(!evh_start(evh1), "start evh");
	evh_register(evh1, acceptor);
	evh_register(evh1, conn);
	connection_write(conn, "Z", 1);
	while (ALOAD(evh1_complete) < 2);
	ASSERT_EQ(ALOAD(evh1_on_connect_val), 1, "connect success");
	ASSERT_EQ(ALOAD(evh1_complete), 2, "2 closed conns");
	release(evh1_complete);
	release(evh1_on_connect_val);
	ASSERT(!evh_stop(evh1), "stop evh");
	ASSERT(evh_stop(evh1), "already stopped");
	evh_destroy(evh1);
	connection_release(acceptor);

	ASSERT_BYTES(0);
}

Test(evh2) {
	i32 ctx = 102;
	u16 port = 0;
	Evh *evh1;
	Connection *acceptor, *conn;
	EvhConfig config = {&ctx, evh1_on_recv, evh1_on_accept, evh1_on_connect,
			    evh1_on_close};

	evh1_complete = alloc(sizeof(u64));
	ASSERT(evh1_complete, "evh1_complete");
	*evh1_complete = 0;

	evh1_on_connect_val = alloc(sizeof(u64));
	ASSERT(evh1_on_connect_val, "evh1_on_connect_val");
	*evh1_on_connect_val = 0;

	acceptor = connection_acceptor(LOCALHOST, port, 10, 0);
	port = connection_acceptor_port(acceptor);
	conn = connection_client(LOCALHOST, port, 0);
	ASSERT(!connection_is_connected(conn), "not connected");
	connection_set_is_connected(conn);
	evh1 = evh_init(&config);
	ASSERT(!evh_start(evh1), "start evh");
	evh_register(evh1, acceptor);
	evh_register(evh1, conn);
	connection_write(conn, "Z", 1);
	while (ALOAD(evh1_complete) < 2);
	ASSERT_EQ(ALOAD(evh1_on_connect_val), 1, "connect success");
	ASSERT_EQ(ALOAD(evh1_complete), 2, "2 closed conns");
	release(evh1_complete);
	release(evh1_on_connect_val);
	ASSERT(!evh_stop(evh1), "stop evh");
	ASSERT(evh_stop(evh1), "already stopped");
	evh_destroy(evh1);
	connection_release(acceptor);

	ASSERT_BYTES(0);
}

Test(evh_fail) {
	i32 ctx = 1;
	EvhConfig config1 = {0};
	EvhConfig config2 = {&ctx, evh1_on_recv, evh1_on_accept,
			     evh1_on_connect, evh1_on_close};

	ASSERT(!evh_init(&config1), "NULL configs");

	_debug_setnonblocking_err = true;
	ASSERT(!evh_init(&config2), "nonblocking fail");
	_debug_setnonblocking_err = false;

	_debug_fail_epoll_create1 = true;
	ASSERT(!evh_init(&config2), "epoll create");
	_debug_fail_epoll_create1 = false;

	_debug_fail_pipe2 = true;
	ASSERT(!evh_init(&config2), "pipe2");
	_debug_fail_pipe2 = false;

	ASSERT_BYTES(0);
}

void proc_acceptor(Evh *evh, Connection *acceptor);

Test(evh_accept_fail) {
	i32 ctx = 102;
	u16 port = 0;
	Evh *evh1;
	Connection *acceptor, *conn;
	EvhConfig config = {&ctx, evh1_on_recv, evh1_on_accept, evh1_on_connect,
			    evh1_on_close};

	evh1_complete = alloc(sizeof(u64));
	ASSERT(evh1_complete, "evh1_complete");
	*evh1_complete = 0;

	evh1_on_connect_val = alloc(sizeof(u64));
	ASSERT(evh1_on_connect_val, "evh1_on_connect_val");
	*evh1_on_connect_val = 0;

	acceptor = connection_acceptor(LOCALHOST, port, 10, 0);
	port = connection_acceptor_port(acceptor);
	conn = connection_client(LOCALHOST, port, 0);
	evh1 = evh_init(&config);
	evh_start(evh1);
	ASSERT(!ALOAD(evh1_on_connect_val), "evh1_on_connect_val");
	connection_set_is_connected(conn);
	_debug_alloc_failure = 1;
	proc_acceptor(evh1, acceptor);
	ASSERT(!ALOAD(evh1_on_connect_val), "evh1_on_connect_val");

	release(evh1_complete);
	release(evh1_on_connect_val);
	evh_stop(evh1);
	evh_destroy(evh1);

	close(connection_socket(conn));
	connection_release(conn);
	connection_release(acceptor);

	ASSERT_BYTES(0);
}

void proc_read(Evh *evh, Connection *conn);

Test(evh_proc_read_fail) {
	u16 port = 0;
	Connection *acceptor = connection_acceptor(LOCALHOST, port, 10, 0);
	Connection *conn =
	    connection_client(LOCALHOST, connection_acceptor_port(acceptor), 0);
	ASSERT(!connection_is_closed(conn), "!is_closed");
	_debug_alloc_failure = 1;
	proc_read(NULL, conn);
	ASSERT(connection_is_closed(conn), "is_closed");

	close(connection_socket(conn));
	connection_release(conn);
	connection_release(acceptor);

	ASSERT_BYTES(0);
}

i32 proc_write(Evh *evh, Connection *conn);

Test(evh_proc_write_fail) {
	u16 port = 0;
	Connection *acceptor = connection_acceptor(LOCALHOST, port, 10, 0);
	Connection *conn =
	    connection_client(LOCALHOST, connection_acceptor_port(acceptor), 0);
	_debug_fail_setsockopt = true;
	ASSERT(proc_write(NULL, conn), "proc_write_fail");
	_debug_fail_setsockopt = false;

	close(connection_socket(conn));
	connection_release(conn);
	connection_release(acceptor);

	ASSERT_BYTES(0);
}

Test(connect_failure) {
	i32 ctx = 102;
	u16 port = 0;
	Evh *evh1;
	Connection *acceptor, *conn;
	EvhConfig config = {&ctx, evh1_on_recv, evh1_on_accept, evh1_on_connect,
			    evh1_on_close};

	evh1_complete = alloc(sizeof(u64));
	ASSERT(evh1_complete, "evh1_complete");
	*evh1_complete = 0;

	evh1_on_connect_val = alloc(sizeof(u64));
	ASSERT(evh1_on_connect_val, "evh1_on_connect_val");
	*evh1_on_connect_val = 0;

	acceptor = connection_acceptor(LOCALHOST, port, 10, 0);
	port = connection_acceptor_port(acceptor);
	conn = connection_client(LOCALHOST, port + 1, 0);
	ASSERT(!connection_is_connected(conn), "not connected");
	evh1 = evh_init(&config);
	ASSERT(!evh_start(evh1), "start evh");
	evh_register(evh1, acceptor);
	evh_register(evh1, conn);
	connection_write(conn, "Z", 1);
	while (ALOAD(evh1_complete) < 1);
	ASSERT_EQ(ALOAD(evh1_on_connect_val), 1, "connect success");
	ASSERT_EQ(ALOAD(evh1_complete), 1, "1 closed conns");
	release(evh1_complete);
	release(evh1_on_connect_val);
	ASSERT(!evh_stop(evh1), "stop evh");
	ASSERT(evh_stop(evh1), "already stopped");
	evh_destroy(evh1);

	connection_release(acceptor);

	ASSERT_BYTES(0);
}

Test(connection_flags) {
	Connection *acceptor = connection_acceptor(LOCALHOST, 0, 10, 0);
	ASSERT(!connection_get_flag(acceptor, CONN_FLAG_USR1), "!USR1");
	connection_set_flag(acceptor, CONN_FLAG_USR1, true);
	ASSERT(connection_get_flag(acceptor, CONN_FLAG_USR1), "USR1");
	connection_set_flag(acceptor, CONN_FLAG_USR1, false);
	ASSERT(!connection_get_flag(acceptor, CONN_FLAG_USR1), "!USR1 (2)");
	connection_release(acceptor);
	ASSERT_BYTES(0);
}

u64 *ws1_success;
u64 *ws1_close_count;

void ws1_on_message(Ws *ws, WsConnection *conn, WsMessage *msg) {
	u8 buf[1024];
	WsMessage resp;
	memcpy(buf, msg->buffer, msg->len);
	buf[msg->len] = 0;
	if (msg->op == 8) {
		ws_close(ws, conn, 1000, "Close requested");
	} else if (msg->op == 2) {
		resp.buffer = buf;
		resp.op = 2;
		resp.len = msg->len;
		ws_send(ws, conn, &resp);
	}
}

void ws1_on_open(Ws *ws, WsConnection *conn) { ASSERT(ws && conn, "non-null"); }

void ws1_on_connect(Ws *ws, WsConnection *conn, int error) {
	ASSERT(ws && conn && !error, "non-null and no errors");
}

void ws1_on_close(Ws *ws, WsConnection *conn) {
	ASSERT(ws && conn, "non_null");
	__add64(ws1_close_count, 1);
}

void wsc1_on_message(Ws *ws, WsConnection *conn, WsMessage *msg) {
	if (msg->op == 8) {
		ws_close(ws, conn, 1000, "Close requested");
	} else {
		ASSERT_EQ(msg->op, 2, "msg->op == 2");
		ASSERT_EQ(msg->len, 4, "msg->len == 4");
		ASSERT(!strcmpn(msg->buffer, "test", 4), "test");
		__add64(ws1_success, 1);
		ws_close(ws, conn, 1000, "Going Away");
	}
}

void wsc1_on_close(Ws *ws, WsConnection *conn) {
	ASSERT(ws && conn, "non-null");
	__add64(ws1_close_count, 1);
}

void wsc1_on_connect(Ws *ws, WsConnection *conn, int error) {
	WsMessage msg;
	ASSERT(!error, "!error");
	msg.op = 2;
	msg.buffer = "test";
	msg.len = 4;
	ws_send(ws, conn, &msg);
}

Test(ws1) {
	Ws *ws;
	WsConfig config = {0};
	Ws *wsc;
	WsConfig client_conf = {0};
	WsConnection *client;
	u16 port;

	ws1_success = alloc(sizeof(u64));
	*ws1_success = 0;
	ws1_close_count = alloc(sizeof(u64));
	*ws1_close_count = 0;

	client_conf.on_message = wsc1_on_message;
	client_conf.on_close = wsc1_on_close;
	client_conf.on_connect = wsc1_on_connect;

	wsc = ws_init(&client_conf);

	config.on_message = ws1_on_message;
	config.on_open = ws1_on_open;
	config.on_close = ws1_on_close;
	config.on_connect = ws1_on_connect;
	ws = ws_init(&config);
	port = ws_port(ws);
	ASSERT(ws, "ws_init");
	ASSERT(!ws_start(ws), "ws_start");

	ASSERT(wsc, "ws_initc");
	ASSERT(!ws_start(wsc), "wsc_start");
	client = ws_connect(wsc, LOCALHOST, port);
	ASSERT(client, "client!=NULL");

	while (!ALOAD(ws1_success));
	ASSERT_EQ(ALOAD(ws1_success), 1, "success=1");
	while (ALOAD(ws1_close_count) != 2);
	ASSERT_EQ(ALOAD(ws1_close_count), 2, "close_count=2");

	release(ws1_success);
	release(ws1_close_count);
	ASSERT(!ws_stop(ws), "ws_stop");
	ws_destroy(ws);
	ASSERT(!ws_stop(wsc), "wsc_stop");
	ws_destroy(wsc);

	ASSERT_BYTES(0);
}
