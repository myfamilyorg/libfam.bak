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
#include <sys/socket.h>

static byte ADDR[4] = {127, 0, 0, 1};
static int OPT = 1;

STATIC void init_robust_ctx(RobustCtx *ctx) {
	struct sockaddr_in address;
	unsigned int addr_len;
	address.sin_family = AF_INET;
	memcpy(&address.sin_addr.s_addr, ADDR, 4);

	while (ctx->port == 0) {
		address.sin_port = 0;
		ctx->sock = socket(AF_INET, SOCK_STREAM, 0);
		if (ctx->sock == -1) {
			print_error("socket");
			continue;
		}
		if (setsockopt(ctx->sock, SOL_SOCKET, SO_REUSEADDR, &OPT,
			       sizeof(OPT)) == -1) {
			print_error("setsocketop");
			close(ctx->sock);
			continue;
		}
		if (bind(ctx->sock, (struct sockaddr *)&address,
			 sizeof(address)) == -1) {
			print_error("bind");
			close(ctx->sock);
			continue;
		}
		addr_len = sizeof(address);
		if (getsockname(ctx->sock, (struct sockaddr *)&address,
				&addr_len) == -1) {
			print_error("getsockname");
			close(ctx->sock);
			continue;
		}
		ctx->port = ntohs(address.sin_port);
		if (ctx->port == 0) {
			print_error("port==0");
			close(ctx->sock);
		}
	}
}
RobustGuard robust_lock(RobustCtx *ctx, RobustLock *lock) {
	uint16_t desired = 0, expected = 0, port;
	struct sockaddr_in address;
	int sock;
	uint64_t counter = 0;
	RobustGuardImpl ret;

	if (ctx->port == 0) init_robust_ctx(ctx);
	memcpy(&address.sin_addr.s_addr, ADDR, 4);
	address.sin_family = AF_INET;
	desired = ctx->port;

start_loop:
	do {
		if (counter++) yield();
		if ((port = __atomic_load_n(lock, __ATOMIC_ACQUIRE)) == 0) {
			expected = 0;
		} else {
			address.sin_port = htons(port);
			sock = socket(AF_INET, SOCK_STREAM, 0);
			if (sock == -1) goto start_loop;
			if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &OPT,
				       sizeof(OPT)) == -1) {
				close(sock);
				goto start_loop;
			}
			if (bind(sock, (struct sockaddr *)&address,
				 sizeof(address)) == 0) {
				close(ctx->sock);
				ctx->sock = sock;
				ctx->port = port;
				break;
			} else {
				close(sock);
				goto start_loop;
			}
		}
	} while (!CAS(lock, &expected, desired));

	ret.lock = lock;
	return ret;
}
void robust_unlock(RobustLock *lock) { ASTORE(lock, 0); }

int robust_ctx_cleanup(RobustCtx *ctx) { return close(ctx->sock); }

void robustguard_cleanup(RobustGuardImpl *rg) {
	if (rg->lock) robust_unlock(rg->lock);
}
