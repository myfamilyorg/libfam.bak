#include <arpa/inet.h>
#include <misc.h>
#include <robust.h>
#include <stdio.h>
#include <sys/socket.h>

#define PORT 9999

int robust_lock(RobustLock *lock) {
	byte addr[4] = {127, 0, 0, 1};
	uint64_t desired = 0, expected = 0;
	struct sockaddr_in address;
	int opt = 1;
	int sock;

	address.sin_family = AF_INET;
	memcpy(&address.sin_addr.s_addr, addr, 4);

	bool is_first = true;
	do {
		if (!is_first) {
			close(sock);
			yield();
		}
		is_first = false;
		sock = socket(AF_INET, SOCK_STREAM, 0);
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		uint64_t cur = __atomic_load_n(lock, __ATOMIC_ACQUIRE);
		uint16_t port = cur >> 48;
		address.sin_port = port;
		if (bind(sock, (struct sockaddr *)&address, sizeof(address)) ==
		    0) {
			// process died or bind assigns us a random port and we
			// are listening
			expected = cur;

			if (!port) {
				unsigned int addr_len = sizeof(address);
				getsockname(sock, (struct sockaddr *)&address,
					    &addr_len);
				port = ntohs(address.sin_port);
			}

			desired = ((uint64_t)port << 48) | sock;
		} else
			continue;

	} while (!__atomic_compare_exchange_n(lock, &expected, desired, false,
					      __ATOMIC_RELEASE,
					      __ATOMIC_RELAXED));

	return 0;
}
int robust_unlock(RobustLock *lock) {
	int fd = __atomic_load_n(lock, __ATOMIC_ACQUIRE);
	close(fd);
	__atomic_store_n(lock, 0, __ATOMIC_RELEASE);
	return 0;
}
