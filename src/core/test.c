#include <alloc.h>
#include <criterion/criterion.h>
#include <error.h>
#include <lock.h>
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
	sched_yield();
	int fd = file("/tmp/data.dat");
	int file_size = lseek(fd, 0, SEEK_END);
	cr_assert_eq(file_size, 0);

	write(fd, "abc", 3);
	int64_t start = micros();
	sleepm(100);
	int64_t diff = micros() - start;
	cr_assert(diff >= 100 * 1000);
	cr_assert(diff <= 1000 * 1000);

	file_size = lseek(fd, 0, SEEK_END);
	cr_assert_eq(file_size, 3);

	int fd2 = file("/tmp/data.dat");
	char buf[4] = {0};
	cr_assert_eq(read(fd2, buf, 4), 3);
	cr_assert_eq(buf[0], 'a');
	cr_assert_eq(buf[1], 'b');
	cr_assert_eq(buf[2], 'c');
	cr_assert_eq(buf[3], '\0');

	unlink("/tmp/data.dat");
}

Test(core, ftrunate) {
	const char *path = "/tmp/data2.dat";
	unlink(path);
	int fd = file(path);
	int file_size = lseek(fd, 0, SEEK_END);
	cr_assert_eq(file_size, 0);

	ftruncate(fd, 1024 * 1024);

	file_size = lseek(fd, 0, SEEK_END);
	cr_assert_eq(file_size, 1024 * 1024);

	close(fd);
	cr_assert_eq(write(fd, "abc", 3), -1);

	unlink(path);
}

Test(core, mmap) {
	const char *path = "/tmp/data3.dat";
	unlink(path);
	int fd = file(path);
	ftruncate(fd, 1024 * 1024);
	char *base =
	    mmap(NULL, 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	cr_assert_eq(base[1], 0);
	cr_assert_eq(base[1024 * 990], 0);
	base[1] = 'a';
	base[1024 * 990] = 'b';
	cr_assert_eq(base[1], 'a');
	cr_assert_eq(base[1024 * 990], 'b');

	close(fd);

	int fd2 = file(path);
	char *base2 =
	    mmap(NULL, 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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

Test(alloc, alloc1) {
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
		// after first loop, we should be incrementing by 32 with each
		// address
		if (last_address)
			cr_assert_eq((size_t)values[i], last_address + 32);
		last_address = (size_t)values[i];
	}

	for (int i = 0; i < size; i++) {
		release(values[i]);
	}
}
