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
} RobustCtx;

static RobustCtx ctx;
static struct sockaddr_in address = {0};

static int opt = 1;
unsigned int addr_len = sizeof(address);

STATIC void robust_connect(uint16_t port) {
	uint8_t addr[4] = {127, 0, 0, 1};
	printf("robust connect %u\n", port);
	address.sin_port = port;
	address.sin_family = AF_INET;
	memcpy(&address.sin_addr.s_addr, addr, 4);

	ctx.sock = socket(AF_INET, SOCK_STREAM, 0);
	if (ctx.sock == -1) {
		perror("socket");
		return;
	}
	if (setsockopt(ctx.sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
	    -1) {
		perror("setsocketop");
		close(ctx.sock);
		return;
	}
	printf("bind ctx.sock=%i\n", ctx.sock);
	if (bind(ctx.sock, (struct sockaddr *)&address, sizeof(address)) ==
	    -1) {
		perror("bind");
		close(ctx.sock);
		return;
	}
	if (listen(ctx.sock, 1)) {
		perror("listen");
		close(ctx.sock);
		return;
	}
	if (getsockname(ctx.sock, (struct sockaddr *)&address, &addr_len) ==
	    -1) {
		perror("getsockname");
		close(ctx.sock);
		return;
	}
	ctx.port = ntohs(address.sin_port);
	if (ctx.port == 0) {
		perror("port==0");
		close(ctx.sock);
	}
}

void robustguard_cleanup(RobustGuardImpl *rg) {
	if (*rg->lock != ctx.port)
		panic(
		    "RobustLock Error: tried to cleanup a lock we don't own!");
	rg->lock = 0;
}

RobustGuard robust_lock(RobustLock *lock) {
	RobustGuardImpl ret;
	int count = 0, sock;
	uint16_t desired, expected = 0, port, prev_port;

	while (!ctx.port) {
		if (++count > 128)
			panic(
			    "RobustLock: too many attempts to create a socket");
		robust_connect(0);
	}

	desired = ctx.port;

start_loop:
	do {
		if ((port = ALOAD(lock)) == 0) {
			expected = 0;
		} else {
			if (port == ctx.port)
				panic(
				    "RobustLock Error: tried to obtain our own "
				    "lock!");
			sock = ctx.sock;      /* Save our old socket */
			prev_port = ctx.port; /* Save our old port */
			robust_connect(port);
			if (ctx.port == port) {
				/* We connected other process died. Close old
				 * sock and continue. */
				close(sock);
				break;
			} else {
				printf("other proc still holds lock\n");
				/* Other process still holds lock */
				ctx.sock = sock;      /* Restore old sock */
				ctx.port = prev_port; /* Restore old port */
				goto start_loop;
			}
		}
	} while (!CAS(lock, &expected, desired));

	ret.lock = lock;
	return ret;
}

void __attribute__((constructor)) robust_init(void) {
	uint8_t addr[4] = {127, 0, 0, 1};

	ctx.sock = 0;
	ctx.port = 0;
	address.sin_family = AF_INET;
	address.sin_port = 0;
	memcpy(&address.sin_addr.s_addr, addr, 4);

	printf("robust init\n");
}

