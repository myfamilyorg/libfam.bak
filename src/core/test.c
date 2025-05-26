#include <criterion/criterion.h>
#include <fcntl.h>
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
	int fd = ocreate("/tmp/data.dat");
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

	int fd2 = ocreate("/tmp/data.dat");
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
	int fd = ocreate(path);
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
	int fd = ocreate(path);
	ftruncate(fd, 1024 * 1024);
	char *base =
	    mmap(NULL, 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	base[1] = 'a';
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
