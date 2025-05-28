#include <arpa/inet.h>
#include <misc.h>
#include <robust.h>
#include <stdio.h>
#include <sys/socket.h>

#define PORT 9999

STATIC int init_robust_ctx(RobustCtx *ctx) {
	byte addr[4] = {127, 0, 0, 1};
	struct sockaddr_in address;
	int opt = 1;

	address.sin_family = AF_INET;
	memcpy(&address.sin_addr.s_addr, addr, 4);

	ctx->sock = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(ctx->sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	bind(ctx->sock, (struct sockaddr *)&address, sizeof(address));
	unsigned int addr_len = sizeof(address);
	getsockname(ctx->sock, (struct sockaddr *)&address, &addr_len);
	ctx->port = ntohs(address.sin_port);

	return 0;
}

int robust_lock(RobustCtx *ctx, RobustLock *lock) {
	byte addr[4] = {127, 0, 0, 1};
	uint64_t desired = 0, expected = 0;
	struct sockaddr_in address;
	int opt = 1;
	int sock;

	if (ctx->sock == 0) init_robust_ctx(ctx);

	address.sin_family = AF_INET;
	memcpy(&address.sin_addr.s_addr, addr, 4);

	bool is_first = true;
	do {
		if (!is_first) {
			yield();
		}
		is_first = false;
		uint64_t cur = __atomic_load_n(lock, __ATOMIC_ACQUIRE);
		if (cur == 0) {
			expected = 0;
			desired = ((uint64_t)ctx->port << 48);
		} else {
			uint16_t port = cur >> 48;
			sock = socket(AF_INET, SOCK_STREAM, 0);
			setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt,
				   sizeof(opt));
			if (bind(sock, (struct sockaddr *)&address,
				 sizeof(address)) == 0) {
				close(ctx->sock);
				ctx->sock = sock;
				ctx->port = port;
				break;
			} else {
				continue;
			}
		}
	} while (!__atomic_compare_exchange_n(lock, &expected, desired, false,
					      __ATOMIC_RELEASE,
					      __ATOMIC_RELAXED));

	return 0;
}
int robust_unlock(RobustLock *lock) {
	__atomic_store_n(lock, 0, __ATOMIC_RELEASE);
	return 0;
}

int robust_ctx_cleanup(RobustCtx *ctx) { return close(ctx->sock); }
