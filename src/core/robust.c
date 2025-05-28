#include <arpa/inet.h>
#include <error.h>
#include <misc.h>
#include <robust.h>
#include <stdio.h>
#include <sys/socket.h>

#define PORT 9999

static byte addr[4] = {127, 0, 0, 1};
static int opt = 1;

STATIC void init_robust_ctx(RobustCtx *ctx) {
	struct sockaddr_in address;
	unsigned int addr_len;
	address.sin_family = AF_INET;
	memcpy(&address.sin_addr.s_addr, addr, 4);

	while (ctx->port == 0) {
		address.sin_port = 0;
		ctx->sock = socket(AF_INET, SOCK_STREAM, 0);
		if (ctx->sock == -1) continue;
		if (setsockopt(ctx->sock, SOL_SOCKET, SO_REUSEADDR, &opt,
			       sizeof(opt)) == -1) {
			close(ctx->sock);
			continue;
		}
		if (bind(ctx->sock, (struct sockaddr *)&address,
			 sizeof(address)) == -1) {
			close(ctx->sock);
			continue;
		}
		addr_len = sizeof(address);
		if (getsockname(ctx->sock, (struct sockaddr *)&address,
				&addr_len) == -1) {
			close(ctx->sock);
			continue;
		}
		ctx->port = ntohs(address.sin_port);
		if (ctx->port <= 0) close(ctx->sock);
	}
}

RobustGuard robust_lock(RobustCtx *ctx, RobustLock *lock) {
	byte addr[4] = {127, 0, 0, 1};
	uint16_t desired = 0, expected = 0;
	struct sockaddr_in address;
	int opt = 1;
	int sock;
	uint64_t counter = 0;

	if (ctx->port == 0) init_robust_ctx(ctx);

	do {
		if (counter++) yield();
		uint16_t cur = __atomic_load_n(lock, __ATOMIC_ACQUIRE);
		if (cur == 0) {
			expected = 0;
			desired = ctx->port;
		} else {
			uint16_t port = cur;

			address.sin_family = AF_INET;
			memcpy(&address.sin_addr.s_addr, addr, 4);

			sock = socket(AF_INET, SOCK_STREAM, 0);
			if (sock == -1) continue;
			if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt,
				       sizeof(opt)) == -1) {
				close(sock);
				continue;
			}
			if (bind(sock, (struct sockaddr *)&address,
				 sizeof(address)) == 0) {
				close(ctx->sock);
				ctx->sock = sock;
				ctx->port = port;
				break;
			} else {
				close(sock);
				continue;
			}
		}
	} while (!__atomic_compare_exchange_n(lock, &expected, desired, false,
					      __ATOMIC_RELEASE,
					      __ATOMIC_RELAXED));

	RobustGuardImpl ret = {.lock = lock};
	return ret;
}
void robust_unlock(RobustLock *lock) {
	__atomic_store_n(lock, 0, __ATOMIC_RELEASE);
}

int robust_ctx_cleanup(RobustCtx *ctx) { return close(ctx->sock); }

void robustguard_cleanup(RobustGuardImpl *rg) {
	if (rg->lock) robust_unlock(rg->lock);
}
