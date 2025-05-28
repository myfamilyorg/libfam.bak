#define _GNU_SOURCE
#include <alloc.h>
#include <arpa/inet.h>
#include <criterion/criterion.h>
#include <error.h>
#include <fcntl.h>
#include <lock.h>
#include <robust.h>
#include <stdio.h>
#include <sys.h>
#include <sys/mman.h>
#include <types.h>

Test(core, types) {
	cr_assert_eq(UINT8_MAX, UINT8_MAX);
	cr_assert_eq(UINT16_MAX, UINT16_MAX);
	cr_assert_eq(UINT32_MAX, UINT32_MAX);
	cr_assert_eq(UINT64_MAX, UINT64_MAX);
	cr_assert_eq(UINT128_MAX, UINT128_MAX);
	cr_assert_eq(INT8_MAX, INT8_MAX);
	cr_assert_eq(INT16_MAX, INT16_MAX);
	cr_assert_eq(INT32_MAX, INT32_MAX);
	cr_assert_eq(INT64_MAX, INT64_MAX);
	cr_assert_eq(INT128_MAX, INT128_MAX);

	cr_assert_eq(UINT8_MIN, UINT8_MIN);
	cr_assert_eq(UINT16_MIN, UINT16_MIN);
	cr_assert_eq(UINT32_MIN, UINT32_MIN);
	cr_assert_eq(UINT64_MIN, UINT64_MIN);
	cr_assert_eq(UINT128_MIN, UINT128_MIN);
	cr_assert_eq(INT8_MIN, INT8_MIN);
	cr_assert_eq(INT16_MIN, INT16_MIN);
	cr_assert_eq(INT32_MIN, INT32_MIN);
	cr_assert_eq(INT64_MIN, INT64_MIN);
	cr_assert_eq(INT128_MIN, INT128_MIN);
	cr_assert_eq(SIZE_MAX, SIZE_MAX);
	cr_assert_eq(false, false);
	cr_assert_eq(true, true);
}

Test(core, sys1) {
	unlink("/tmp/data.dat");
	yield();
	int fd = file("/tmp/data.dat");
	int file_size = fsize(fd);
	cr_assert_eq(file_size, 0);

	write(fd, "abc", 3);
	int64_t start = micros();
	sleepm(100);
	int64_t diff = micros() - start;
	cr_assert(diff >= 100 * 1000);
	cr_assert(diff <= 1000 * 1000);

	file_size = fsize(fd);
	cr_assert_eq(file_size, 3);

	int fd2 = file("/tmp/data.dat");
	char buf[4] = {0};
	cr_assert_eq(read(fd2, buf, 4), 3);
	cr_assert_eq(buf[0], 'a');
	cr_assert_eq(buf[1], 'b');
	cr_assert_eq(buf[2], 'c');
	cr_assert_eq(buf[3], '\0');

	close(fd);
	close(fd2);

	unlink("/tmp/data.dat");
}

Test(core, fcntl) {
	const char *path = "/tmp/fcntl.dat";
	unlink(path);
	int fd = file(path);
	fresize(fd, 1024);
	cr_assert_eq(fsize(fd), 1024);

	int min_fd = 100;
	int new_fd = fcntl(fd, F_DUPFD, min_fd);
	cr_assert(new_fd >= 100);
	cr_assert(new_fd != fd);
	cr_assert_eq(fsize(new_fd), 1024);

	close(fd);
	close(new_fd);

	unlink(path);
}

Test(core, ftrunate) {
	const char *path = "/tmp/data2.dat";
	unlink(path);
	int fd = file(path);
	int file_size = fsize(fd);
	cr_assert_eq(file_size, 0);

	fresize(fd, 1024 * 1024);

	file_size = fsize(fd);
	cr_assert_eq(file_size, 1024 * 1024);

	close(fd);
	cr_assert_eq(write(fd, "abc", 3), -1);

	unlink(path);
}

Test(core, mmap) {
	const char *path = "/tmp/data3.dat";
	unlink(path);
	int fd = file(path);
	fresize(fd, 1024 * 1024);
	char *base = fmap(fd);
	cr_assert_eq(base[1], 0);
	cr_assert_eq(base[1024 * 990], 0);
	base[1] = 'a';
	base[1024 * 990] = 'b';
	cr_assert_eq(base[1], 'a');
	cr_assert_eq(base[1024 * 990], 'b');

	close(fd);

	int fd2 = file(path);
	char *base2 = fmap(fd2);
	cr_assert_eq(base2[1], 'a');
	cr_assert_eq(base2[1024 * 990], 'b');
	cr_assert_eq(base2[1024 * 991], 0);

	close(fd2);
	unlink(path);
}

Test(core, testforkpipe) {
	int fds[2], fdsback[2];
	pipe(fds);
	pipe(fdsback);

	int pid = fork();

	if (pid == 0) {
		char buf[10] = {0};
		cr_assert_eq(read(fds[0], buf, 10), 5);
		cr_assert_eq(buf[0], '0');
		cr_assert_eq(buf[1], '1');
		cr_assert_eq(buf[2], '2');
		cr_assert_eq(buf[3], '3');
		cr_assert_eq(buf[4], '4');
		cr_assert_eq(buf[5], '\0');

		cr_assert_eq(write(fdsback[1], "abcd", 4), 4);
		exit(0);
	} else {
		char buf[10] = {0};
		cr_assert_eq(write(fds[1], "01234", 5), 5);
		cr_assert_eq(read(fdsback[0], buf, 10), 4);
		cr_assert_eq(buf[0], 'a');
		cr_assert_eq(buf[1], 'b');
		cr_assert_eq(buf[2], 'c');
		cr_assert_eq(buf[3], 'd');
		cr_assert_eq(buf[4], '\0');
	}
}

Test(core, multiplex) {
	int my_data = 101;
	Event events[10];
	int m = multiplex();
	int fds[2];
	char buf[10];

	cr_assert(m > 0);
	cr_assert_eq(mwait(m, events, 10, 1), 0);

	pipe(fds);
	cr_assert(fds[0] > 0);
	cr_assert(fds[1] > 0);
	mregister(m, fds[0], MULTIPLEX_FLAG_READ, &my_data);
	write(fds[1], "test", 4);

	cr_assert_eq(mwait(m, events, 10, 1), 1);
	cr_assert(event_is_read(events[0]));
	cr_assert(!event_is_write(events[0]));
	cr_assert_eq(event_attachment(events[0]), &my_data);
	cr_assert_eq(read(fds[0], buf, 10), 4);
	cr_assert_eq(buf[0], 't');
	cr_assert_eq(buf[1], 'e');
	cr_assert_eq(buf[2], 's');
	cr_assert_eq(buf[3], 't');

	close(fds[0]);
	close(fds[1]);

	close(m);
}

Test(core, lock) {
	Lock l1 = LOCK_INIT;
	{
		LockGuard lg1 = lock_read(&l1);
		cr_assert_eq(l1, 1);
		LockGuard lg2 = lock_read(&l1);
		cr_assert_eq(l1, 2);
	}
	cr_assert_eq(l1, 0);

	{
		LockGuard lg1 = lock_write(&l1);
		// write flag is set (highest bit)
		cr_assert_eq(l1, (0x1UL << 63));
	}
	cr_assert_eq(l1, 0);
}

Test(core, lock2) {
	void *base = smap(16384);
	Lock *l1 = (Lock *)base;
	int *value = base + 8;
	cr_assert_eq(*value, 0);

	int pid = fork();
	if (pid == 0) {
		sleepm(10);
		{
			LockGuard lg = lock_write(l1);
			*value = 1;
		}
		exit(0);
	} else {
		while (true) {
			sleepm(1);
			LockGuard lg = lock_read(l1);
			if (*value) break;
		}
	}
}

typedef struct {
	Lock lock;
	int value;
} SharedState;

Test(core, lock3) {
	void *base = smap(sizeof(SharedState));
	SharedState *state = (SharedState *)base;
	state->lock = LOCK_INIT;
	state->value = 0;

	int pid = fork();
	if (pid == 0) {
		{
			while (true) {
				sleepm(1);
				LockGuard lg = lock_write(&state->lock);
				if (state->value) {
					state->value = 2;
					sleepm(100);
					break;
				}
			}
			state->value = 3;
		}
		exit(0);
	} else {
		sleepm(10);
		{
			LockGuard lg = lock_write(&state->lock);
			state->value = 1;
		}
		while (true) {
			sleepm(1);
			LockGuard lg = lock_write(&state->lock);
			if (state->value == 3) break;
		}
	}
}

Test(core, lock4) {
	void *base = smap(sizeof(SharedState));
	SharedState *state = (SharedState *)base;
	state->lock = LOCK_INIT;
	state->value = 0;

	if (fork()) {
		while (true) {
			LockGuard lg = lock_write(&state->lock);
			if (state->value % 2 == 0) state->value++;
			if (state->value >= 10) break;
		}

		{
			LockGuard lg = lock_write(&state->lock);
			state->value = -1;
		}

		while (true) {
			LockGuard lg = lock_write(&state->lock);
			if (state->value == 0) break;
		}
	} else {
		while (true) {
			LockGuard lg = lock_write(&state->lock);
			if (state->value % 2 == 1) state->value++;
			if (state->value == -1) break;
		}

		{
			LockGuard lg = lock_write(&state->lock);
			state->value = 0;
		}
		exit(0);
	}
}

#ifndef MEMSAN
#define MEMSAN 0
#endif /* MEMSAN */
#if MEMSAN == 1
uint64_t get_mmaped_bytes();
#endif /* MEMSAN */

Test(core, alloc1) {
	size_t last_address;
	char *test1 = alloc(1000 * 1000 * 5);
	release(test1);

	void *a = alloc(10);

	last_address = 0;
	for (int i = 0; i < 32000; i++) {
		char *test2 = alloc(10);
		// after first loop, we should just be reusing addresses
		if (last_address)
			cr_assert_eq((size_t)test2, last_address);
		else
			cr_assert(a != test2);
		cr_assert(test2 != NULL);
		test2[0] = 'a';
		test2[1] = 'b';
		release(test2);
		last_address = (size_t)test2;
	}

	release(a);

	// use a different size
	int size = 1000;
	char *values[size];
	last_address = 0;
	for (int i = 0; i < size; i++) {
		values[i] = alloc(32);
		// after first loop, we should be incrementing by 32
		// with each address
		if (last_address)
			cr_assert_eq((size_t)values[i], last_address + 32);
		last_address = (size_t)values[i];
	}

#if MEMSAN == 1
	cr_assert(get_mmaped_bytes());
#endif /* MEMSAN */

	for (int i = 0; i < size; i++) {
		release(values[i]);
	}
#if MEMSAN == 1
	cr_assert_eq(get_mmaped_bytes(), 0);
#endif /* MEMSAN */
}

Test(alloc, up_and_down) {
#if MEMSAN == 1
	cr_assert_eq(get_mmaped_bytes(), 0);
#endif /* MEMSAN */

	size_t last_address;
	char *test1 = alloc(1000 * 1000 * 5);
	release(test1);

	void *a = alloc(10);

	a = resize(a, 1000 * 1000 * 5 * 10);

	release(a);

	last_address = 0;
	for (int i = 0; i < 32000; i++) {
		char *test2 = alloc(10);
		// after first loop, we should just be reusing addresses
		if (last_address)
			cr_assert_eq((size_t)test2, last_address);
		else
			cr_assert(a != test2);
		cr_assert(test2 != NULL);
		test2[0] = 'a';
		test2[1] = 'b';

#if MEMSAN == 1
		cr_assert(get_mmaped_bytes());
#endif /* MEMSAN */

		release(test2);
		last_address = (size_t)test2;
#if MEMSAN == 1
		cr_assert_eq(get_mmaped_bytes(), 0);
#endif /* MEMSAN */
	}

#if MEMSAN == 1
	cr_assert_eq(get_mmaped_bytes(), 0);
#endif /* MEMSAN */
}

Test(alloc, slab_impl) {
	int size = 127;
	char *values[size];
	size_t last_address = 0;
	for (int i = 0; i < size; i++) {
		values[i] = alloc(2048);
		if (last_address)
			cr_assert_eq((size_t)values[i], last_address + 2048);
		last_address = (size_t)values[i];
	}

	void *next = alloc(2048);
	// This creates a new mmap so the address will not be last +
	// 2048
	cr_assert((size_t)next != last_address + 2048);

	void *next2 = alloc(2040);  // still in the 2048 block size
	cr_assert_eq((size_t)next2, (size_t)next + 2048);

	release(next);
	release(next2);

	for (int i = 0; i < size; i++) {
		release(values[i]);
	}
#if MEMSAN == 1
	cr_assert_eq(get_mmaped_bytes(), 0);
#endif /* MEMSAN */
}

Test(alloc, slab_release) {
	int size = 127;
	char *values[size];
	size_t last_address = 0;
	for (int i = 0; i < size; i++) {
		values[i] = alloc(2048);
		if (last_address)
			cr_assert_eq((size_t)values[i], last_address + 2048);
		last_address = (size_t)values[i];
	}

	// free several slabs
	release(values[18]);
	release(values[125]);
	release(values[15]);

	// ensure last pointer is being used and gets us the correct
	// bits.
	void *next = alloc(2048);
	void *next2 = alloc(2048);
	void *next3 = alloc(2048);

	cr_assert_eq((size_t)next, (size_t)values[15]);
	cr_assert_eq((size_t)next2, (size_t)values[18]);
	cr_assert_eq((size_t)next3, (size_t)values[125]);

	values[15] = next;
	values[18] = next2;
	values[125] = next3;

	for (int i = 0; i < size; i++) {
		release(values[i]);
	}
#if MEMSAN == 1
	cr_assert_eq(get_mmaped_bytes(), 0);
#endif /* MEMSAN */
}

Test(alloc, multi_chunk_search) {
	for (int x = 0; x < 4; x++) {
		int size = 1000;
		char *values[size];
		size_t last_address = 0;
		for (int i = 0; i < size; i++) {
			values[i] = alloc(2048);
		}

		// free several slabs
		release(values[418]);
		release(values[825]);
		release(values[15]);

		void *next = alloc(2048);
		void *next2 = alloc(2048);
		void *next3 = alloc(2048);

		cr_assert_eq((size_t)next, (size_t)values[15]);
		cr_assert_eq((size_t)next2, (size_t)values[418]);
		cr_assert_eq((size_t)next3, (size_t)values[825]);

		values[15] = next;
		values[825] = next2;
		values[418] = next3;

		for (int i = 0; i < size; i++) {
			release(values[i]);
		}
#if MEMSAN == 1
		cr_assert_eq(get_mmaped_bytes(), 0);
#endif /* MEMSAN */
	}
}

Test(alloc, larger_allocations) {
	size_t max = 16384 * 3;
	for (size_t i = 2049; i < max; i++) {
		char *ptr = alloc(i);
		cr_assert(ptr);
		for (size_t j = 0; j < 10; j++) ptr[j] = 'x';
		for (size_t j = 0; j < 10; j++) cr_assert_eq(ptr[j], 'x');
		release(ptr);
	}
#if MEMSAN == 1
	cr_assert_eq(get_mmaped_bytes(), 0);
#endif /* MEMSAN */
}

Test(alloc, test_realloc) {
	void *tmp;

	void *slab1 = alloc(8);
	void *slab2 = alloc(16);
	void *slab3 = alloc(32);
	void *slab4 = alloc(64);
	void *slab5 = alloc(128);

	tmp = resize(slab1, 16);
	cr_assert_eq((size_t)tmp, (size_t)slab2 + 16);
	slab1 = tmp;

	tmp = resize(slab1, 32);
	cr_assert_eq((size_t)tmp, (size_t)slab3 + 32);
	slab1 = tmp;

	tmp = resize(slab1, 64);
	cr_assert_eq((size_t)tmp, (size_t)slab4 + 64);
	slab1 = tmp;

	tmp = resize(slab1, 128);
	cr_assert_eq((size_t)tmp, (size_t)slab5 + 128);
	slab1 = tmp;

	tmp = resize(slab1, 1024 * 1024 * 6);
	/* Should be a CHUNK_SIZE aligned + 16 pointer */
	cr_assert_eq(((size_t)tmp - 16) % CHUNK_SIZE, 0);
	slab1 = tmp;

	/* now go down */
	tmp = resize(slab1, 16);
	cr_assert_eq((size_t)tmp, (size_t)slab2 + 16);
	slab1 = tmp;

	release(slab1);
	release(slab2);
	release(slab3);
	release(slab4);
	release(slab5);
#if MEMSAN == 1
	cr_assert_eq(get_mmaped_bytes(), 0);
#endif /* MEMSAN */
}

Test(core, test_align) {
	size_t alloc_sz = 16;
	void *x = alloc(8);
	cr_assert_eq((size_t)x % 8, 0);
	release(x);
	for (int i = 0; i < 8; i++) {
		void *x = alloc(alloc_sz);
		cr_assert_eq((size_t)x % 16, 0);
		alloc_sz *= 2;
		release(x);
	}
#if MEMSAN == 1
	cr_assert_eq(get_mmaped_bytes(), 0);
#endif /* MEMSAN */
}

Test(core, socket1) {
	int port = 0;
	byte addr[4] = {127, 0, 0, 1};
	int s1 = socket(AF_INET, SOCK_STREAM, 0);
	int s2 = socket(AF_INET, SOCK_STREAM, 0);
	int opt = 1;
	struct sockaddr_in address;
	int flags;
	int res;
	uint32_t ip;

	cr_assert(s1 > 0);
	cr_assert(s2 > 0);

	setsockopt(s1, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	setsockopt(s1, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

	address.sin_family = AF_INET;
	ip = (addr[0] << 24) | (addr[1] << 16) | (addr[2] << 8) | addr[3];
	address.sin_addr.s_addr = htonl(ip);
	address.sin_port = htons(port);

	ip = (addr[0] << 24) | (addr[1] << 16) | (addr[2] << 8) | addr[3];
	address.sin_addr.s_addr = htonl(ip);
	address.sin_port = htons(port);

	res = bind(s1, (struct sockaddr *)&address, sizeof(address));
	cr_assert(res >= 0);

	res = listen(s1, 10);

	socklen_t addr_len = sizeof(address);
	res = getsockname(s1, (struct sockaddr *)&address, &addr_len);
	cr_assert(res >= 0);
	port = ntohs(address.sin_port);
	cr_assert(port > 0);

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	memcpy(&serv_addr.sin_addr.s_addr, addr, 4);

	res = connect(s2, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	cr_assert(res >= 0);

	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int s3 = accept(s1, (struct sockaddr *)&client_addr, &client_len);
	cr_assert(s3 > 0);

	cr_assert_eq(write(s3, "test", 4), 4);
	char buf[10] = {0};
	cr_assert_eq(read(s2, buf, 10), 4);
	cr_assert_eq(buf[0], 't');
	cr_assert_eq(buf[1], 'e');
	cr_assert_eq(buf[2], 's');
	cr_assert_eq(buf[3], 't');

	close(s1);
	close(s2);
	close(s3);
}

Test(core, robust1) {
	RobustCtx ctx = ROBUST_CTX_INIT;
	RobustLock l1 = ROBUST_LOCK_INIT;
	RobustGuard rg = robust_lock(&ctx, &l1);
}

typedef struct {
	RobustLock lock;
	int value;
	int value2;
} RobustSharedState;

Test(core, robust2) {
	void *base = smap(sizeof(RobustSharedState));
	RobustSharedState *state = (RobustSharedState *)base;
	state->lock = ROBUST_LOCK_INIT;
	state->value = 0;

	if (fork()) {
		RobustCtx ctx = ROBUST_CTX_INIT;
		sleepm(100);
		while (true) {
			RobustGuard rg = robust_lock(&ctx, &state->lock);
			if (state->value == 1) break;
		}
	} else {
		RobustCtx ctx = ROBUST_CTX_INIT;
		// ensure we don't unlock by using RobustGuardImpl (without
		// RAII)
		RobustGuardImpl rg = robust_lock(&ctx, &state->lock);
		sleepm(300);
		state->value++;
		exit(0);
	}
}

Test(core, robust3) {
	void *base = smap(sizeof(RobustSharedState));
	RobustSharedState *state = (RobustSharedState *)base;
	state->lock = ROBUST_LOCK_INIT;
	state->value = 0;

	if (fork()) {
		RobustCtx ctx = ROBUST_CTX_INIT;
		while (true) {
			RobustGuard rg = robust_lock(&ctx, &state->lock);
			if (state->value == 1) break;
		}
		cr_assert_eq(state->value, 1);
	} else {
		RobustCtx ctx = ROBUST_CTX_INIT;
		RobustGuard rg = robust_lock(&ctx, &state->lock);
		state->value++;
		exit(0);
	}
}

Test(core, robust4) {
	RobustCtx ctx = ROBUST_CTX_INIT;
	void *base = smap(sizeof(RobustSharedState));
	RobustSharedState *state = (RobustSharedState *)base;
	state->lock = ROBUST_LOCK_INIT;
	state->value = 0;
	state->value2 = 0;

	if (fork()) {
		while (true) {
			RobustGuard rg = robust_lock(&ctx, &state->lock);
			cr_assert(state->lock);
			if (state->value % 2 == 0) state->value++;
			if (state->value >= 11) break;
		}
		state->value2 = 1;
		while (true) {
			RobustGuard rg = robust_lock(&ctx, &state->lock);
			cr_assert(state->lock);
			if (state->value2 == 0) break;
		}
		cr_assert_eq(state->value2, 0);
		robust_ctx_cleanup(&ctx);
	} else {
		while (true) {
			RobustGuard rg = robust_lock(&ctx, &state->lock);
			cr_assert(state->lock);
			if (state->value % 2 == 1) state->value++;
			if (state->value >= 11) break;
		}
		while (true) {
			RobustGuard rg = robust_lock(&ctx, &state->lock);
			cr_assert(state->lock);
			if (state->value2 == 1) break;
		}
		state->value2 = 0;
		robust_ctx_cleanup(&ctx);
		exit(0);
	}
}
