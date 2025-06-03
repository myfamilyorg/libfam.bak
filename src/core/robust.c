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

#include <arpa/inet.h>
#include <atomic.h>
#include <error.h>
#include <misc.h>
#include <robust.h>
#include <stdio.h>
#include <sys/socket.h>

typedef struct {
	int sock;
	uint16_t port;
	uint64_t last_lock_micros;
	uint64_t lock_count;
} RobustCtx;

#define IDLE_MAX_MILLIS 5000
#define CHECK_DISCONNECT_MILLIS 10000

static int write_u64(int fd, uint64_t value) {
	char buffer[20];
	int len = 0;
	int i, j;
	ssize_t bytes_written;

	if (value == 0) {
		buffer[0] = '0';
		len = 1;
	} else {
		while (value > 0) {
			if (len >= 20) {
				err = EOVERFLOW;
				return -1;
			}
			buffer[len++] = '0' + (value % 10);
			value /= 10;
		}
	}

	for (i = 0, j = len - 1; i < j; i++, j--) {
		char temp = buffer[i];
		buffer[i] = buffer[j];
		buffer[j] = temp;
	}

	bytes_written = write(fd, buffer, len);
	if (bytes_written == -1) return -1;
	if (bytes_written != len) {
		err = EIO;
		return -1;
	}

	return 0;
}

static RobustCtx ctx;
static struct sockaddr_in address = {0};
static int opt = 1;
unsigned int addr_len = sizeof(address);

STATIC void robust_check_disconnect(void) {
	uint64_t now = micros();
	if (!ctx.lock_count &&
	    now - ctx.last_lock_micros > (IDLE_MAX_MILLIS * 1000)) {
		close(ctx.sock);
		ctx.sock = 0;
		ctx.port = 0;
	} else {
		timeout(robust_check_disconnect, CHECK_DISCONNECT_MILLIS);
	}
}

STATIC uint16_t robust_connect(uint16_t port) {
	int sock;
	address.sin_port = ntohs(port);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("socket");
		return 0;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
	    -1) {
		perror("setsocketop");
		close(sock);
		return 0;
	}
	if (bind(sock, (struct sockaddr *)&address, sizeof(address)) == -1) {
		close(sock);
		return 0;
	}
	if (listen(sock, 1)) {
		close(sock);
		return 0;
	}
	addr_len = sizeof(address);
	if (getsockname(sock, (struct sockaddr *)&address, &addr_len) == -1) {
		perror("getsockname");
		close(sock);
		return 0;
	}
	port = ntohs(address.sin_port);
	if (port == 0) {
		perror("port==0");
		close(sock);
	} else {
		if (ctx.sock) close(ctx.sock);
		ctx.sock = sock;
	}
	return port;
}

void robustguard_cleanup(RobustGuardImpl *rg) {
	uint64_t v;
	if ((v = ALOAD(rg->lock)) != ctx.port) {
		write(2, "lock: ", 6);
		write_u64(2, v);
		write(2, "\nctx.port: ", 11);
		write_u64(2, ctx.port);
		write(2, "\n", 1);
		panic(
		    "RobustLock Error: tried to cleanup a lock we don't own!");
	}
	ctx.lock_count--;
	ASTORE(rg->lock, 0);
}

RobustGuard robust_lock(RobustLock *lock) {
	RobustGuardImpl ret;
	int count = 0, nport;
	uint64_t do_yield = 0;
	uint16_t desired, expected = 0, port;

	ctx.lock_count++;

	while (!ctx.port) {
		ctx.port = robust_connect(0);
		if (++count > 128)
			panic(
			    "RobustLock: too many attempts to create a socket");
	}
	desired = ctx.port;
start_loop:
	do {
		if (do_yield++) yield();
		if ((port = ALOAD(lock)) == 0)
			expected = 0;
		else {
			if (port == ctx.port)
				panic(
				    "RobustLock Error: tried to obtain our own "
				    "lock!");
			nport = robust_connect(port);
			if (nport == port) {
				ctx.port = nport;
				break;
			} else
				goto start_loop;
		}
	} while (!CAS(lock, &expected, desired));

	ctx.last_lock_micros = micros();
	/* Check if we should disconnect in the specified time */
	timeout(robust_check_disconnect, CHECK_DISCONNECT_MILLIS);
	ret.lock = lock;
	return ret;
}

void __attribute__((constructor)) robust_init(void) {
	uint8_t addr[4] = {127, 0, 0, 1};
	ctx.sock = ctx.port = 0;
	address.sin_family = AF_INET;
	address.sin_port = 0;
	memcpy(&address.sin_addr.s_addr, addr, 4);
}

