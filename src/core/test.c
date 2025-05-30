#include <criterion/criterion.h>
#include <error.h>
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

