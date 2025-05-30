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

#include <criterion/criterion.h>
#include <error.h>
#include <evh.h>
#include <socket.h>
#include <sys.h>

Test(net, socket1) {
	byte addr[4] = {127, 0, 0, 1};
	Socket s1, s2, s3, s4;
	int port = socket_listen(&s1, addr, 0, 10);
	cr_assert(port > 0);
	sleepm(10);
	cr_assert(!socket_connect(&s2, addr, port));
	cr_assert(s2 > 0);
	while (true) {
		if (socket_accept(&s1, &s3)) {
			if (err != EAGAIN) cr_assert(false);
			sleepm(1);
		} else
			break;
	}

	cr_assert(s3 > 0);

	/* Test an error */
	err = SUCCESS;
	socket_accept(&s2, &s4);
	cr_assert_eq(err, EINVAL);

	int total_written = 0;

	while (total_written < 4) {
		int v = write(s2, "test", 4);
		cr_assert(v >= 0);
		total_written += v;
	}

	char buf[10];
	int total_read = 0;

	while (total_read < 4) {
		int v = read(s3, buf + total_read, 10);
		if (v < 0 && err == EAGAIN) continue;
		cr_assert(v >= 0);
		total_read += v;
	}
	cr_assert_eq(total_read, 4);
	cr_assert_eq(total_written, 4);
	cr_assert_eq(buf[0], 't');
	cr_assert_eq(buf[1], 'e');
	cr_assert_eq(buf[2], 's');
	cr_assert_eq(buf[3], 't');

	close(s1);
	close(s2);
	close(s3);
}

int on_recv(void *ctx, struct Connection *conn, size_t len) {
	char buf[conn->data.inbound.rbuf_offset + 1];
	memcpy(buf, conn->data.inbound.rbuf, conn->data.inbound.rbuf_offset);
	buf[conn->data.inbound.rbuf_offset] = 0;
	printf("recv[%i] callback len = %lu full_buf='%s'\n", conn->socket, len,
	       buf);
	return 0;
}

int on_accept(void *ctx, struct Connection *conn) {
	printf("accept conn callback %i\n", conn->socket);
	return 0;
}

int on_close(void *ctx, struct Connection *conn) {
	printf("conn close callback %i\n", conn->socket);
	return 0;
}

Test(net, evh) {
	Evh evh;
	Socket socket;
	byte addr[4] = {127, 0, 0, 1};
	Connection conn;

	int port = socket_listen(&socket, addr, 10000, 10);
	printf("port=%i\n", port);
	conn.socket = socket;
	conn.conn_type = Acceptor;
	conn.data.acceptor.on_recv = on_recv;
	conn.data.acceptor.on_accept = on_accept;
	conn.data.acceptor.on_close = on_close;

	cr_assert(!evh_start(&evh));
	cr_assert(!evh_register(&evh, &conn));
	//	sleepm(1000 * 1000 * 1000);
	evh_stop(&evh);
	printf("evh stopped!\n");
	sleepm(1000);
	printf("sleep done\n");
}
