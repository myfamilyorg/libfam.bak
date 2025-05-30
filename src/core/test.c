#include <atomic.h>
#include <criterion/criterion.h>
#include <error.h>
#include <fcntl.h>
#include <lock.h>
#include <stdio.h>
#include <sys.h>

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
	int v = read(fd2, buf, 4);
	cr_assert_eq(v, 3);
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

typedef struct {
	Lock lock1;
	Lock lock2;
	int value1;
	int value2;
	int value3;
} SharedStateData;

// reader waits for writer
Test(core, locka) {
	void *base = smap(sizeof(SharedStateData));
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	state->value3 = 0;

	if (fork()) {
		while (true) {
			LockGuard lg1 = lock_read(&state->lock1);
			if (ALOAD(&state->value1)) break;
		}
		cr_assert_eq(AADD(&state->value3, 1), 1);
		LockGuard lg2 = lock_read(&state->lock2);
		cr_assert_eq(ALOAD(&state->value2), 1);
		cr_assert_eq(AADD(&state->value3, 1), 3);
	} else {
		{
			LockGuard lg2 = lock_write(&state->lock2);
			cr_assert_eq(AADD(&state->value3, 1), 0);
			{
				LockGuard lg1 = lock_write(&state->lock1);
				ASTORE(&state->value1, 1);
			}
			sleepm(10);
			ASTORE(&state->value2, 1);
			cr_assert_eq(AADD(&state->value3, 1), 2);
		}
		exit(0);
	}
}

// writer blocks writer
Test(core, lockb) {
	void *base = smap(sizeof(SharedStateData));
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	state->value3 = 0;

	if (fork()) {
		while (true) {
			LockGuard lg1 = lock_read(&state->lock1);
			if (ALOAD(&state->value1)) break;
		}
		cr_assert_eq(AADD(&state->value3, 1), 1);
		LockGuard lg2 = lock_write(&state->lock2);
		cr_assert_eq(ALOAD(&state->value2), 1);
		cr_assert_eq(AADD(&state->value3, 1), 3);
	} else {
		{
			LockGuard lg2 = lock_write(&state->lock2);
			cr_assert_eq(AADD(&state->value3, 1), 0);
			{
				LockGuard lg1 = lock_write(&state->lock1);
				ASTORE(&state->value1, 1);
			}
			sleepm(10);
			ASTORE(&state->value2, 1);
			cr_assert_eq(AADD(&state->value3, 1), 2);
		}
		exit(0);
	}
}

// Multiple reader blocks writer
Test(core, lockc) {
	void *base = smap(sizeof(SharedStateData));
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	state->value3 = 0;

	if (fork()) {
		while (true) {
			LockGuard lg1 = lock_read(&state->lock1);
			if (ALOAD(&state->value1)) break;
		}
		LockGuard lg2 = lock_write(&state->lock2);
		ASTORE(&state->value2, 1);
		cr_assert_eq(ALOAD(&state->value3), 1);
	} else {
		{
			LockGuard lg2 = lock_read(&state->lock2);
			{
				LockGuard lg1 = lock_write(&state->lock1);
				ASTORE(&state->value1, 1);
			}
			sleepm(10);
			cr_assert_eq(ALOAD(&state->value2), 0);
			ASTORE(&state->value3, 1);
		}
		exit(0);
	}
}

// Multiple readers prevent writer from getting lock
Test(core, lockd) {
	void *base = smap(sizeof(SharedStateData));
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	state->value3 = 0;

	if (fork()) {
		while (true) {
			LockGuard lg1 = lock_read(&state->lock1);
			if (ALOAD(&state->value1)) break;
		}
		LockGuard lg2 = lock_write(&state->lock2);
		ASTORE(&state->value2, 1);
		cr_assert_eq(ALOAD(&state->value3), 3);
	} else {
		{
			LockGuard lga = lock_read(&state->lock2);
			{
				LockGuard lgb = lock_read(&state->lock2);
				{
					LockGuard lgc =
					    lock_read(&state->lock2);
					{
						LockGuard lg1 =
						    lock_write(&state->lock1);
						ASTORE(&state->value1, 1);
					}

					sleepm(10);
					AADD(&state->value3, 1);
				}
				sleepm(10);
				AADD(&state->value3, 1);
			}
			sleepm(10);
			AADD(&state->value3, 1);
		}
		exit(0);
	}
}

// Write starvation prevention test
Test(core, locke) {
	void *base = smap(sizeof(SharedStateData));
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	state->value3 = 0;

	if (fork()) {
		while (true) {
			LockGuard lg1 = lock_read(&state->lock1);
			if (ALOAD(&state->value1) == 2) break;
		}

		LockGuard lg2 = lock_write(&state->lock2);
		// writer gets access first
		cr_assert_eq(AADD(&state->value2, 1), 1);

	} else {
		if (fork()) {
			{
				LockGuard lg2 = lock_read(&state->lock2);
				AADD(&state->value1, 1);
				// ensure that the writer has requested write
				// before proceeding
				while ((ALOAD(&state->lock2) & (0x1UL << 62)) ==
				       0);
			}
			exit(0);
		} else {
			{
				AADD(&state->value1, 1);
				// ensure that the writer has requested write
				// before proceeding
				while ((ALOAD(&state->lock2) & (0x1UL << 62)) ==
				       0);
				cr_assert_eq(AADD(&state->value2, 1), 0);
				LockGuard lg2 = lock_read(&state->lock2);
				// reader gets access second (starvation
				// prevention)
				cr_assert_eq(AADD(&state->value2, 1), 2);
			}
			exit(0);
		}
	}
}
