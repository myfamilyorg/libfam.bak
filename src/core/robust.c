#include <arpa/inet.h>
#include <error.h>
#include <misc.h>
#include <robust.h>
#include <stdio.h>
#include <sys/socket.h>

#define PORT 9999

STATIC void init_robust_ctx(RobustCtx *ctx) {
	byte addr[4] = {127, 0, 0, 1};
	int opt = 1;

	while (ctx->port == 0) {
		struct sockaddr_in address;
		address.sin_family = AF_INET;
		address.sin_port = 0;
		memcpy(&address.sin_addr.s_addr, addr, 4);

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
		unsigned int addr_len = sizeof(address);
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
	uint64_t desired = 0, expected = 0;
	struct sockaddr_in address;
	int opt = 1;
	int sock;

	if (ctx->port == 0) init_robust_ctx(ctx);

	address.sin_family = AF_INET;
	memcpy(&address.sin_addr.s_addr, addr, 4);

	uint64_t counter = 0;
	do {
		if (counter++) yield();
		uint64_t cur = __atomic_load_n(lock, __ATOMIC_ACQUIRE);
		if (cur == 0) {
			expected = 0;
			desired = ((uint64_t)ctx->port << 48);
		} else {
			uint16_t port = cur >> 48;
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
int robust_unlock(RobustLock *lock) {
	__atomic_store_n(lock, 0, __ATOMIC_RELEASE);
	return 0;
}

int robust_ctx_cleanup(RobustCtx *ctx) { return close(ctx->sock); }

void robustguard_cleanup(RobustGuardImpl *rg) {
	if (rg->lock) robust_unlock(rg->lock);
}
