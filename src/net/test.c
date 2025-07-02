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
