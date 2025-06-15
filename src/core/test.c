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

#include <alloc.H>
#include <atomic.H>
#include <env.H>
#include <error.H>
#include <init.H>
#include <limits.H>
#include <misc.H>
#include <syscall_const.H>
#include <test.H>

typedef struct {
	int value1;
	int value2;
	int value3;
	int value4;
	int value5;
	uint32_t uvalue1;
	uint32_t uvalue2;
} SharedStateData;

Test(env) {
	ASSERT(getenv("TEST_PATTERN"), "getenv1");
	ASSERT(!getenv("__TEST_PATTERNS__"), "getenv2");
	setenv("__TEST_PATTERNS__", "1", true);
	ASSERT(getenv("__TEST_PATTERNS__"), "getenv3");
	setenv("__TEST_PATTERNS__", "2", true);
	ASSERT(!strcmp(getenv("__TEST_PATTERNS__"), "2"), "tp=2");
	unsetenv("__TEST_PATTERNS__");
	ASSERT(!getenv("__TEST_PATTERNS__"), "getenv4");
}

Test(two1) {
	void *base = smap(sizeof(SharedStateData));
	int cpid;
	SharedStateData *state = (SharedStateData *)base;
	state->value1 = 0;
	state->value2 = 0;
	if ((cpid = two())) {
		while (!ALOAD(&state->value1));
		state->value2++;
	} else {
		ASTORE(&state->value1, 1);
		exit(0);
	}
	ASSERT(state->value2, "store");
	waitid(P_PID, cpid, NULL, WEXITED);
	munmap(base, sizeof(SharedStateData));
}

Test(futex1) {
	void *base = smap(sizeof(SharedStateData));
	int cpid;
	SharedStateData *state = (SharedStateData *)base;
	state->uvalue1 = (uint32_t)0;
	if ((cpid = two())) {
		while (state->uvalue1 == 0) {
			futex(&state->uvalue1, FUTEX_WAIT, 0, NULL, NULL, 0);
		}
		ASSERT(state->uvalue1, "value1");
		state->value2++;
	} else {
		state->uvalue1 = 1;
		futex(&state->uvalue1, FUTEX_WAKE, 1, NULL, NULL, 0);
		exit(0);
	}
	waitid(P_PID, cpid, NULL, WEXITED);
	ASSERT(state->value2, "value2");
	munmap(base, sizeof(SharedStateData));
}

uint32_t tfunv1 = 0;
uint32_t tfunv2 = 0;
uint32_t tfunv3 = 0;

void tfun1(void) { __add32(&tfunv1, 1); }

void tfun2(void) { __add32(&tfunv2, 1); }

void tfun3(void) { __add32(&tfunv3, 1); }

Test(timeout1) {
	ASSERT(!timeout(tfun1, 100), "tfun1");
	while (!ALOAD(&tfunv1));
	ASSERT_EQ(ALOAD(&tfunv1), 1, "complete");
}

Test(timeout2) {
	ASTORE(&tfunv1, 0);
	ASTORE(&tfunv2, 0);
	ASTORE(&tfunv3, 0);

	timeout(tfun1, 100);
	timeout(tfun2, 200);
	while (!ALOAD(&tfunv1)) {
	}
	{
		ASSERT_EQ(ALOAD(&tfunv1), 1, "1");
		ASSERT_EQ(ALOAD(&tfunv2), 0, "2");
	}
	while (!ALOAD(&tfunv2)) {
	}
	ASSERT_EQ(ALOAD(&tfunv2), 1, "3");
}

Test(timeout3) {
	int pid, i;
	SharedStateData *state =
	    (SharedStateData *)smap(sizeof(SharedStateData));

	ASTORE(&tfunv1, 0);
	ASTORE(&tfunv2, 0);
	ASTORE(&tfunv3, 0);
	ASTORE(&state->value1, 0);

	timeout(tfun1, 100);
	timeout(tfun2, 200);
	pid = two();
	if (pid) {
		timeout(tfun3, 150);
		for (i = 0; i < 3; i++) sleepm(200);
		ASSERT_EQ(ALOAD(&tfunv1), 1, "v1 1");
		ASSERT_EQ(ALOAD(&tfunv2), 1, "v2 1");
		ASSERT_EQ(ALOAD(&tfunv3), 1, "v3 1");
		while (!ALOAD(&state->value1));
	} else {
		timeout(tfun3, 150);
		for (i = 0; i < 3; i++) sleepm(200);
		ASSERT_EQ(ALOAD(&tfunv1), 0, "two v1");
		ASSERT_EQ(ALOAD(&tfunv2), 0, "two v2");
		ASSERT_EQ(ALOAD(&tfunv3), 1, "two v3");
		ASTORE(&state->value1, 1);
		exit(0);
	}

	waitid(P_PID, pid, NULL, WEXITED);

	munmap(state, sizeof(SharedStateData));
}

Test(timeout4) {
	int i;
	ASSERT(timeout(NULL, 100), "timeout NULL");
	ASTORE(&tfunv1, 0);

	for (i = 0; i < 32; i++) {
		ASSERT(!timeout(tfun1, 10), "timout");
	}

	err = 0;
	ASSERT(timeout(tfun1, 10), "overflow");
	ASSERT_EQ(err, EOVERFLOW, "overflow err check");

	while (true) {
		yield();
		if (tfunv1 != 32) continue;
		ASSERT_EQ(tfunv1, 32, "tfunv");
		break;
	}
}

#define CHUNK_SIZE (1024 * 1024 * 4)

Test(alloc1) {
	char *t1, *t2, *t3, *t4, *t5;

	t1 = alloc(CHUNK_SIZE);
	t4 = alloc(8);
	t5 = alloc(8);
	t4[0] = 1;
	t5[0] = 1;
	ASSERT(t4 != t5, "t4 != t5");
	release(t4);
	release(t5);
	t4 = alloc(8);
	t5 = alloc(8);
	ASSERT_BYTES(16 + CHUNK_SIZE);
	release(t4);
	release(t5);
	release(t1);
	ASSERT_BYTES(0);

	t1 = alloc(1024 * 1024);
	ASSERT(t1, "t1");
	t2 = alloc(1024 * 1024);
	ASSERT(t2, "t2");
	t3 = alloc(1024 * 1024);
	ASSERT(t3, "t3");
	t4 = alloc(1024 * 1024);
	ASSERT(t4, "t4");
	ASSERT_BYTES(4 * 1024 * 1024);
	release(t1);
	release(t2);
	release(t3);
	release(t4);
	ASSERT(!alloc(CHUNK_SIZE + 1), "over alloc fail");
	err = 0;
	release(NULL);
	ASSERT_EQ(err, EINVAL, "release null");

	ASSERT_BYTES(0);
}

Test(resize) {
	char *t1 = resize(NULL, 8), *t2, *t3, *t4, *t5;
	ASSERT(t1, "t1 not null");
	t2 = resize(t1, 16);
	ASSERT(t1 != t2, "t1!=t2");
	t3 = resize(t2, 8);
	ASSERT_EQ(t1, t3, "t1 == t3");

	ASSERT_EQ(resize(t3, 0), NULL, "resize 0");
	t3 = alloc(8);
	ASSERT_EQ(resize(t3, CHUNK_SIZE + 1), NULL, "greater than CHUNK_SIZE");
	release(t3);

	ASSERT_EQ(resize((void *)100, 10), NULL, "out of range");

	t4 = alloc(CHUNK_SIZE);
	t5 = resize(t4, 32);
	ASSERT(t5, "resize down from chunk");
	release(t5);
}

Test(atomic) {
	uint32_t x = 2, a = 0, b = 1;
	uint64_t y = 2;

	__or32(&x, 1);
	ASSERT_EQ(x, 3, "or32");

	__or64(&y, 1);
	ASSERT_EQ(y, 3, "or64");

	__sub64(&y, 1);
	ASSERT_EQ(y, 2, "sub64");

	ASSERT(!__cas32(&a, &b, 0), "cas32");

	__sub32(&x, 1);
	ASSERT_EQ(x, 2, "x=2");
	__and32(&x, 3);
	ASSERT_EQ(x, 2, "x=2and");

	__and64(&y, 1);
	ASSERT(!y, "!y");
}

uint128_t __umodti3(uint128_t a, uint128_t b);
uint128_t __udivti3(uint128_t a, uint128_t b);

Test(misc) {
	const char *s = "abcdefghi";
	char bufbig[200];
	char buf1[10];
	char buf2[10];
	char sx[100];
	int len;
	int128_t v, v2;
	uint128_t y, x;

	ASSERT(strcmpn("1abc", "1def", 4) < 0, "strcmpn1");
	ASSERT(strcmpn("1ubc", "1def", 4) > 0, "strcmpn2");

	ASSERT_EQ(substr(s, "def") - s, 3, "strstr");
	memset(buf1, 'b', 10);
	memset(buf2, 'a', 10);
	memorymove(buf2, buf1, 8);
	ASSERT_EQ(buf2[0], 'b', "b");
	ASSERT_EQ(buf2[9], 'a', "a");
	byteszero(buf2, 10);
	ASSERT_EQ(buf2[0], 0, "byteszero");

	memset(buf2, 'a', 10);
	memset((uint8_t *)buf2 + 3, 'b', 7);
	memorymove((uint8_t *)buf2 + 3, (uint8_t *)buf2, 7);
	ASSERT_EQ(buf2[3], 'a', "b0");

	ASSERT_EQ(uint128_t_to_string(buf1, 100), 3, "100 len");
	ASSERT(!strcmpn(buf1, "100", 3), "100");
	ASSERT_EQ(uint128_t_to_string(buf1, 0), 1, "0 len");
	ASSERT_EQ(buf1[0], '0', "0 value");
	ASSERT_EQ(int128_t_to_string(buf1, -10), 3, "-10 len");
	ASSERT_EQ(buf1[0], '-', "-10 value");
	ASSERT_EQ(buf1[1], '1', "-10 value2");
	ASSERT_EQ(buf1[2], '0', "-10 value3");
	ASSERT_EQ(int128_t_to_string(buf1, 10), 2, "10 len");

	ASSERT_EQ(string_to_uint128(" 123", 4), 123, "123");
	err = 0;
	ASSERT_EQ(string_to_uint128("", 0), 0, "EINVAL");
	ASSERT_EQ(err, EINVAL, "EINVALVAL");
	ASSERT_EQ(string_to_uint128(" ", 1), 0, "0 err");
	ASSERT_EQ(string_to_uint128("i", 1), 0, "i err");
	ASSERT_EQ(
	    string_to_uint128("340282366920938463463374607431768211456", 39), 0,
	    "overflow");
	ASSERT_EQ(
	    string_to_uint128("3402823669209384634633746074317682114560", 40),
	    0, "overflow");

	err = 0;
	v = string_to_int128("123", 3);
	ASSERT_EQ(v, 123, "v=123");
	ASSERT_EQ(err, SUCCESS, "success");
	ASSERT_EQ(string_to_int128("123", 3), 123, "123");
	ASSERT_EQ(string_to_int128("", 0), 0, "0 len");
	err = 0;
	v2 = string_to_int128("111", 3);
	ASSERT_EQ(v2, 111, "111");
	ASSERT_EQ(string_to_int128(" 1234", 5), 1234, "1234");
	ASSERT_EQ(string_to_int128(" ", 1), 0, "0");
	ASSERT_EQ(string_to_int128("-3", 2), -3, "-3");
	ASSERT_EQ(string_to_int128("+5", 2), 5, "+5");
	ASSERT_EQ(string_to_int128("-", 1), 0, "-");
	len = uint128_t_to_string(sx, UINT128_MAX);
	ASSERT_EQ(string_to_int128(sx, len), 0, "overflow int128");

	ASSERT_EQ(double_to_string(buf1, 1.2, 1), 3, "double to str 1.2");
	buf1[3] = 0;
	ASSERT(!strcmp(buf1, "1.2"), "1.2");
	ASSERT_EQ(double_to_string(buf1, -4.2, 1), 4, "double to str -4.2");
	buf1[4] = 0;
	ASSERT(!strcmp(buf1, "-4.2"), "-4.2");
	ASSERT_EQ(double_to_string(bufbig, 0.123, 3), 5, "0.123");

	ASSERT_EQ(__umodti3(11, 10), 1, "11 % 10");
	ASSERT_EQ(__udivti3(31, 10), 3, "31 / 10");
	y = ((uint128_t)1) << 64;
	x = __umodti3(y, ((uint128_t)1) << 66);
	ASSERT_EQ(x, y, "big mod");

	ASSERT_EQ(double_to_string(bufbig, 0.0, 3), 1, "d2str0.0");
	ASSERT_EQ(bufbig[0], '0', "0");

	ASSERT(!strcmp(substrn("abcdefghi", "def", 9), "defghi"), "substrn");
}

Test(udivti3) {
	uint128_t a, b, q;

	a = 16;
	b = ((uint128_t)1) << 65;
	q = __udivti3(a, b);
	ASSERT_EQ(q, 0, "small_a_div");

	a = 100;
	b = 10;
	q = __udivti3(a, b);
	ASSERT_EQ(q, 10, "64bit_small_div");

	a = ((uint128_t)1) << 80;
	b = 2;
	q = __udivti3(a, b);
	ASSERT_EQ(q, ((uint128_t)1) << 79, "64bit_large_div");

	a = ((uint128_t)100) << 65;
	b = ((uint128_t)10) << 65;
	q = __udivti3(a, b);
	ASSERT_EQ(q, 10, "128bit_div");

	a = ((uint128_t)1) << 65;
	b = ((uint128_t)1) << 65;
	q = __udivti3(a, b);
	ASSERT_EQ(q, 1, "128bit_div");

	a = ((uint128_t)1) << 100;
	b = ((uint128_t)1) << 10;
	q = __udivti3(a, b);
	ASSERT_EQ(q, ((uint128_t)1) << 90, "large_a_small_b_div");
}
Test(umodti3) {
	uint128_t a, b, r;

	a = 16;
	b = ((uint128_t)1) << 65;
	r = __umodti3(a, b);
	ASSERT_EQ(r, 16, "small_a_mod");

	a = 100;
	b = 7;
	r = __umodti3(a, b);
	ASSERT_EQ(r, 2, "64bit_small_mod");

	a = ((uint128_t)1) << 80;
	b = 3;
	r = __umodti3(a, b);
	ASSERT_EQ(r, 1, "64bit_large_mod");

	a = ((uint128_t)100) << 65;
	b = ((uint128_t)7) << 65;
	r = __umodti3(a, b);
	ASSERT_EQ(r, ((uint128_t)2) << 65, "128bit_mod");

	a = ((uint128_t)1) << 65;
	b = ((uint128_t)1) << 65;
	r = __umodti3(a, b);
	ASSERT_EQ(r, 0, "equal_mod");

	a = ((uint128_t)1) << 100;
	b = ((uint128_t)1) << 10;
	r = __umodti3(a, b);
	ASSERT_EQ(r, 0, "large_a_small_b_mod");
}

Test(double_to_string) {
	char buf[64]; /* Ensure sufficient size */
	uint64_t len;

	/* NaN (lines 310–316) */
	len = double_to_string(buf, 0.0 / 0.0, 6);
	ASSERT(!strcmp(buf, "nan"), "nan");
	ASSERT_EQ(len, 3, "nan_len");

	/* Positive infinity (lines 319–329) */
	len = double_to_string(buf, 1.0 / 0.0, 6);
	ASSERT(!strcmp(buf, "inf"), "inf");
	ASSERT_EQ(len, 3, "inf_len");

	/* Negative infinity (lines 319–329) */
	len = double_to_string(buf, -1.0 / 0.0, 6);
	ASSERT(!strcmp(buf, "-inf"), "neg_inf");
	ASSERT_EQ(len, 4, "neg_inf_len");

	/* Integer overflow (lines 37–47) */
	len = double_to_string(buf, 1.8446744073709551616e19, 6); /* 2^64 */
	ASSERT(!strcmp(buf, "inf"), "overflow_inf");
	ASSERT_EQ(len, 3, "overflow_inf_len");

	/* Zero (lines 340–343) */
	len = double_to_string(buf, 0.0, 6);
	ASSERT(!strcmp(buf, "0"), "zero");
	ASSERT_EQ(len, 1, "zero_len");

	/* Negative zero */
	len = double_to_string(buf, -0.0, 6);
	ASSERT(!strcmp(buf, "0"), "neg_zero");
	ASSERT_EQ(len, 1, "neg_zero_len");

	/* Positive integer (lines 351–364) */
	len = double_to_string(buf, 123.0, 0);
	ASSERT(!strcmp(buf, "123"), "int_pos");
	ASSERT_EQ(len, 3, "int_pos_len");

	/* Negative integer */
	len = double_to_string(buf, -123.0, 0);
	ASSERT(!strcmp(buf, "-123"), "int_neg");

	ASSERT_EQ(len, 4, "int_neg_len");

	/* Fractional number (lines 368–385) */
	len = double_to_string(buf, 123.456789, 6);
	ASSERT(!strcmp(buf, "123.456789"), "frac");
	ASSERT_EQ(len, 10, "frac_len");

	/* Negative fractional */
	len = double_to_string(buf, -123.456789, 6);
	ASSERT(!strcmp(buf, "-123.456789"), "neg_frac");
	ASSERT_EQ(len, 11, "neg_frac_len");

	/* Rounding (lines 376–385) */
	len = double_to_string(buf, 0.9999995, 6);
	ASSERT(!strcmp(buf, "1"), "round_up");
	ASSERT_EQ(len, 1, "round_up_len");

	/* Trailing zero trim (lines 388–390) */
	len = double_to_string(buf, 123.4000, 6);
	ASSERT(!strcmp(buf, "123.4"), "trim_zeros");
	ASSERT_EQ(len, 5, "trim_zeros_len");

	/* Remove decimal point (line 393) */
	len = double_to_string(buf, 123.0000001, 6);
	ASSERT(!strcmp(buf, "123"), "remove_decimal");
	ASSERT_EQ(len, 3, "remove_decimal_len");

	len = double_to_string(buf, 123.456789123456789, 18);
	buf[len] = 0;
	ASSERT(!strcmp(buf, "123.45678912345678668"), "max_decimals");
	ASSERT_EQ(len, 21, "max_decimals_len");

	/* Negative max_decimals (line 347) */
	len = double_to_string(buf, 123.456, -1);
	ASSERT(!strcmp(buf, "123"), "neg_decimals");
	ASSERT_EQ(len, 3, "neg_decimals_len");
}

Test(colors) {
	const char *test_file = "/tmp/colortest";
	int fd;

	if (getenv("NO_COLOR")) return; /* Can't test without colors */
	unlink(test_file);
	fd = file(test_file);
	write(fd, RED, strlen(RED));
	ASSERT_EQ(fsize(fd), 5, "red size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, YELLOW, strlen(YELLOW));
	ASSERT_EQ(fsize(fd), 5, "yellow size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, BLUE, strlen(BLUE));
	ASSERT_EQ(fsize(fd), 5, "blue size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, MAGENTA, strlen(MAGENTA));
	ASSERT_EQ(fsize(fd), 5, "magennta size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, GREEN, strlen(GREEN));
	ASSERT_EQ(fsize(fd), 5, "green size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, DIMMED, strlen(DIMMED));
	ASSERT_EQ(fsize(fd), 4, "dimmed size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, CYAN, strlen(CYAN));
	ASSERT_EQ(fsize(fd), 5, "cyan size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, BRIGHT_RED, strlen(BRIGHT_RED));
	ASSERT_EQ(fsize(fd), 5, "bright_red size");
	close(fd);
	unlink(test_file);

	setenv("NO_COLOR", "1", true);

	fd = file(test_file);
	write(fd, RED, strlen(RED));
	ASSERT_EQ(fsize(fd), 0, "red size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, YELLOW, strlen(YELLOW));
	ASSERT_EQ(fsize(fd), 0, "yellow size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, BLUE, strlen(BLUE));
	ASSERT_EQ(fsize(fd), 0, "blue size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, MAGENTA, strlen(MAGENTA));
	ASSERT_EQ(fsize(fd), 0, "magennta size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, GREEN, strlen(GREEN));
	ASSERT_EQ(fsize(fd), 0, "green size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, DIMMED, strlen(DIMMED));
	ASSERT_EQ(fsize(fd), 0, "dimmed size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, CYAN, strlen(CYAN));
	ASSERT_EQ(fsize(fd), 0, "cyan size");
	close(fd);
	unlink(test_file);

	fd = file(test_file);
	write(fd, BRIGHT_RED, strlen(BRIGHT_RED));
	ASSERT_EQ(fsize(fd), 0, "bright_red size");
	close(fd);
	unlink(test_file);

	unsetenv("NO_COLOR");
}

int *__error(void);

Test(errors) {
	ASSERT(!strcmp("Success", error_string(0)), "success");
	ASSERT(!strcmp("Operation not permitted", error_string(EPERM)),
	       "eperm");
	ASSERT(!strcmp("No such file or directory", error_string(ENOENT)),
	       "enoent");
	ASSERT(!strcmp("No such process", error_string(ESRCH)), "esrch");
	ASSERT(!strcmp("Interrupted system call", error_string(EINTR)),
	       "eintr");
	ASSERT(!strcmp("Input/output error", error_string(EIO)), "eio");
	ASSERT(!strcmp("No such device or address", error_string(ENXIO)),
	       "enxio");
	ASSERT(!strcmp("Argument list too long", error_string(E2BIG)), "e2big");
	ASSERT(!strcmp("Exec format error", error_string(ENOEXEC)), "enoexec");
	ASSERT(!strcmp("Bad file descriptor", error_string(EBADF)), "ebadf");
	ASSERT(!strcmp("No child processes", error_string(ECHILD)), "echild");
	ASSERT(
	    !strcmp("Resource temporarily unavailable", error_string(EAGAIN)),
	    "eagain");
	ASSERT(!strcmp("Out of memory", error_string(ENOMEM)), "enomem");
	ASSERT(!strcmp("Permission denied", error_string(EACCES)), "eacces");
	ASSERT(!strcmp("Bad address", error_string(EFAULT)), "efault");
	ASSERT(!strcmp("Block device required", error_string(ENOTBLK)),
	       "enotblk");
	ASSERT(!strcmp("Device or resource busy", error_string(EBUSY)),
	       "ebusy");
	ASSERT(!strcmp("File exists", error_string(EEXIST)), "eexist");
	ASSERT(!strcmp("Invalid cross-device link", error_string(EXDEV)),
	       "exdev");
	ASSERT(!strcmp("No such device", error_string(ENODEV)), "enodev");
	ASSERT(!strcmp("Not a directory", error_string(ENOTDIR)), "enotdir");
	ASSERT(!strcmp("Is a directory", error_string(EISDIR)), "eisdir");
	ASSERT(!strcmp("Invalid argument", error_string(EINVAL)), "einval");
	ASSERT(!strcmp("File table overflow", error_string(ENFILE)), "enfile");
	ASSERT(!strcmp("Too many open files", error_string(EMFILE)), "emfile");
	ASSERT(!strcmp("Not a typewriter", error_string(ENOTTY)), "enotty");
	ASSERT(!strcmp("Text file busy", error_string(ETXTBSY)), "etxtbsy");
	ASSERT(!strcmp("File too large", error_string(EFBIG)), "efbig");
	ASSERT(!strcmp("No space left on device", error_string(ENOSPC)),
	       "enospc");
	ASSERT(!strcmp("Illegal seek", error_string(ESPIPE)), "espipe");
	ASSERT(!strcmp("Read-only file system", error_string(EROFS)), "erofs");
	ASSERT(!strcmp("Too many links", error_string(EMLINK)), "emlink");
	ASSERT(!strcmp("Broken pipe", error_string(EPIPE)), "epipe");
	ASSERT(
	    !strcmp("Math argument out of domain of func", error_string(EDOM)),
	    "edom");
	ASSERT(!strcmp("Math result not representable", error_string(ERANGE)),
	       "erange");
	ASSERT(!strcmp("Resource deadlock would occur", error_string(EDEADLK)),
	       "edeadlk");
	ASSERT(!strcmp("File name too long", error_string(ENAMETOOLONG)),
	       "enametoolong");
	ASSERT(!strcmp("No record locks available", error_string(ENOLCK)),
	       "enolck");
	ASSERT(!strcmp("Invalid system call number", error_string(ENOSYS)),
	       "enosys");
	ASSERT(!strcmp("Directory not empty", error_string(ENOTEMPTY)),
	       "enotempty");
	ASSERT(
	    !strcmp("Too many symbolic links encountered", error_string(ELOOP)),
	    "eloop");
	ASSERT(!strcmp("No message of desired type", error_string(ENOMSG)),
	       "enomsg");
	ASSERT(!strcmp("Identifier removed", error_string(EIDRM)), "eidrm");
	ASSERT(!strcmp("Channel number out of range", error_string(ECHRNG)),
	       "echrng");
	ASSERT(!strcmp("Level 2 not synchronized", error_string(EL2NSYNC)),
	       "el2nsync");
	ASSERT(!strcmp("Level 3 halted", error_string(EL3HLT)), "el3hlt");
	ASSERT(!strcmp("Level 3 reset", error_string(EL3RST)), "el3rst");
	ASSERT(!strcmp("Link number out of range", error_string(ELNRNG)),
	       "elnrng");
	ASSERT(!strcmp("Protocol driver not attached", error_string(EUNATCH)),
	       "eunatch");
	ASSERT(!strcmp("No CSI structure available", error_string(ENOCSI)),
	       "enocsi");
	ASSERT(!strcmp("Level 2 halted", error_string(EL2HLT)), "el2hlt");
	ASSERT(!strcmp("Invalid exchange", error_string(EBADE)), "ebade");
	ASSERT(!strcmp("Invalid request descriptor", error_string(EBADR)),
	       "ebadr");
	ASSERT(!strcmp("Exchange full", error_string(EXFULL)), "exfull");
	ASSERT(!strcmp("No anode", error_string(ENOANO)), "enoano");
	ASSERT(!strcmp("Invalid request code", error_string(EBADRQC)),
	       "ebadrqc");
	ASSERT(!strcmp("Invalid slot", error_string(EBADSLT)), "ebadslt");
	ASSERT(!strcmp("Bad font file format", error_string(EBFONT)), "ebfont");
	ASSERT(!strcmp("Device not a stream", error_string(ENOSTR)), "enostr");
	ASSERT(!strcmp("No data available", error_string(ENODATA)), "enodata");
	ASSERT(!strcmp("Timer expired", error_string(ETIME)), "etime");
	ASSERT(!strcmp("Out of streams resources", error_string(ENOSR)),
	       "enosr");
	ASSERT(!strcmp("Machine is not on the network", error_string(ENONET)),
	       "enonet");
	ASSERT(!strcmp("Package not installed", error_string(ENOPKG)),
	       "enopkg");
	ASSERT(!strcmp("Object is remote", error_string(EREMOTE)), "eremote");
	ASSERT(!strcmp("Link has been severed", error_string(ENOLINK)),
	       "enolink");
	ASSERT(!strcmp("Advertise error", error_string(EADV)), "eadv");
	ASSERT(!strcmp("Srmount error", error_string(ESRMNT)), "esrmnt");
	ASSERT(!strcmp("Communication error on send", error_string(ECOMM)),
	       "ecomm");
	ASSERT(!strcmp("Protocol error", error_string(EPROTO)), "eproto");
	ASSERT(!strcmp("Multihop attempted", error_string(EMULTIHOP)),
	       "emultihop");
	ASSERT(!strcmp("RFS specific error", error_string(EDOTDOT)), "edotdot");
	ASSERT(!strcmp("Not a data message", error_string(EBADMSG)), "ebadmsg");
	ASSERT(!strcmp("Value too large for defined data type",
		       error_string(EOVERFLOW)),
	       "eoverflow");
	ASSERT(!strcmp("Name not unique on network", error_string(ENOTUNIQ)),
	       "enotuniq");
	ASSERT(!strcmp("File descriptor in bad state", error_string(EBADFD)),
	       "ebadfd");
	ASSERT(!strcmp("Remote address changed", error_string(EREMCHG)),
	       "eremchg");
	ASSERT(!strcmp("Can not access a needed shared library",
		       error_string(ELIBACC)),
	       "elibacc");
	ASSERT(!strcmp("Accessing a corrupted shared library",
		       error_string(ELIBBAD)),
	       "elibbad");
	ASSERT(!strcmp("lib section in a.out corrupted", error_string(ELIBSCN)),
	       "elibscn");
	ASSERT(!strcmp("Attempting to link in too many shared libraries",
		       error_string(ELIBMAX)),
	       "elibmax");
	ASSERT(!strcmp("Cannot exec a shared library directly",
		       error_string(ELIBEXEC)),
	       "elibexec");
	ASSERT(!strcmp("Illegal byte sequence", error_string(EILSEQ)),
	       "eilseq");
	ASSERT(!strcmp("Interrupted system call should be restarted",
		       error_string(ERESTART)),
	       "erestart");
	ASSERT(!strcmp("Streams pipe error", error_string(ESTRPIPE)),
	       "estrpipe");
	ASSERT(!strcmp("Too many users", error_string(EUSERS)), "eusers");
	ASSERT(
	    !strcmp("Socket operation on non-socket", error_string(ENOTSOCK)),
	    "enotsock");
	ASSERT(
	    !strcmp("Destination address required", error_string(EDESTADDRREQ)),
	    "edestaddrreq");
	ASSERT(!strcmp("Message too long", error_string(EMSGSIZE)), "emsgsize");
	ASSERT(
	    !strcmp("Protocol wrong type for socket", error_string(EPROTOTYPE)),
	    "eprototype");
	ASSERT(!strcmp("Protocol not available", error_string(ENOPROTOOPT)),
	       "enoprotoopt");
	ASSERT(!strcmp("Protocol not supported", error_string(EPROTONOSUPPORT)),
	       "eprotonosupport");
	ASSERT(
	    !strcmp("Socket type not supported", error_string(ESOCKTNOSUPPORT)),
	    "esocktnosupport");
	ASSERT(!strcmp("Operation not supported on transport endpoint",
		       error_string(EOPNOTSUPP)),
	       "eopnotsupp");
	ASSERT(!strcmp("Protocol family not supported",
		       error_string(EPFNOSUPPORT)),
	       "epfnosupport");
	ASSERT(!strcmp("Address family not supported by protocol",
		       error_string(EAFNOSUPPORT)),
	       "eafnosupport");
	ASSERT(!strcmp("Address already in use", error_string(EADDRINUSE)),
	       "eaddrinuse");
	ASSERT(!strcmp("Cannot assign requested address",
		       error_string(EADDRNOTAVAIL)),
	       "eaddrnotavail");
	ASSERT(!strcmp("Network is down", error_string(ENETDOWN)), "enetdown");
	ASSERT(!strcmp("Network is unreachable", error_string(ENETUNREACH)),
	       "enetunreach");
	ASSERT(!strcmp("Network dropped connection because of reset",
		       error_string(ENETRESET)),
	       "enetreset");
	ASSERT(!strcmp("Software caused connection abort",
		       error_string(ECONNABORTED)),
	       "econnaborted");
	ASSERT(!strcmp("Connection reset by peer", error_string(ECONNRESET)),
	       "econnreset");
	ASSERT(!strcmp("No buffer space available", error_string(ENOBUFS)),
	       "enobufs");
	ASSERT(!strcmp("Transport endpoint is already connected",
		       error_string(EISCONN)),
	       "eisconn");
	ASSERT(!strcmp("Transport endpoint is not connected",
		       error_string(ENOTCONN)),
	       "enotconn");
	ASSERT(!strcmp("Cannot send after transport endpoint shutdown",
		       error_string(ESHUTDOWN)),
	       "eshutdown");
	ASSERT(!strcmp("Too many references: cannot splice",
		       error_string(ETOOMANYREFS)),
	       "etoomanyrefs");
	ASSERT(!strcmp("Connection timed out", error_string(ETIMEDOUT)),
	       "etimedout");
	ASSERT(!strcmp("Connection refused", error_string(ECONNREFUSED)),
	       "econnrefused");
	ASSERT(!strcmp("Host is down", error_string(EHOSTDOWN)), "ehostdown");
	ASSERT(!strcmp("No route to host", error_string(EHOSTUNREACH)),
	       "ehostunreach");
	ASSERT(!strcmp("Operation already in progress", error_string(EALREADY)),
	       "ealready");
	ASSERT(!strcmp("Operation now in progress", error_string(EINPROGRESS)),
	       "einprogress");
	ASSERT(!strcmp("Stale file handle", error_string(ESTALE)), "estale");
	ASSERT(!strcmp("Structure needs cleaning", error_string(EUCLEAN)),
	       "euclean");
	ASSERT(!strcmp("Not a XENIX named type file", error_string(ENOTNAM)),
	       "enotnam");
	ASSERT(!strcmp("No XENIX semaphores available", error_string(ENAVAIL)),
	       "enavail");
	ASSERT(!strcmp("Is a named type file", error_string(EISNAM)), "eisnam");
	ASSERT(!strcmp("Remote I/O error", error_string(EREMOTEIO)),
	       "eremoteio");
	ASSERT(!strcmp("Quota exceeded", error_string(EDQUOT)), "edquot");
	ASSERT(!strcmp("No medium found", error_string(ENOMEDIUM)),
	       "enomedium");
	ASSERT(!strcmp("Wrong medium type", error_string(EMEDIUMTYPE)),
	       "emediumtype");
	ASSERT(!strcmp("Operation canceled", error_string(ECANCELED)),
	       "ecanceled");
	ASSERT(!strcmp("Required key not available", error_string(ENOKEY)),
	       "enokey");
	ASSERT(!strcmp("Key has expired", error_string(EKEYEXPIRED)),
	       "ekeyexpired");
	ASSERT(!strcmp("Key has been revoked", error_string(EKEYREVOKED)),
	       "ekeyrevoked");
	ASSERT(
	    !strcmp("Key was rejected by service", error_string(EKEYREJECTED)),
	    "ekeyrejected");
	ASSERT(!strcmp("Unknown error", error_string(1000)), "unknown");

	err = 0;
	ASSERT_EQ(*__error(), 0, "__error");
	perror_set_no_write(true);
	perror("test");
	perror_set_no_write(false);
}

Test(snprintf) {
	char buf[1024];
	int len = snprintf(NULL, 0, "test%i", 1);
	ASSERT_EQ(len, 5, "len=5");
	ASSERT_EQ(snprintf(buf, sizeof(buf), "%i%s%s 3%d", 1, "xyz", "ghi", 4),
		  10, "len=10");
	ASSERT(!strcmp(buf, "1xyzghi 34"), "1xyzghi 34");

	ASSERT_EQ(snprintf(buf, sizeof(buf), "xxx%iyyy", -12),
		  (int)strlen("xxx-12yyy"), "len");
	ASSERT(!strcmp(buf, "xxx-12yyy"), "xxx-12yyy");
	ASSERT_EQ(snprintf(buf, sizeof(buf), "abc%cdef", 'v'),
		  (int)strlen("abcvdef"), "len2");
	ASSERT(!strcmp(buf, "abcvdef"), "abcvdef");
	ASSERT_EQ(snprintf(buf, sizeof(buf), "%x %d", 10, 10),
		  (int)strlen("a 10"), "len3");
	ASSERT(!strcmp(buf, "a 10"), "a 10");

	ASSERT_EQ(snprintf(buf, sizeof(buf), "%"), 1, "percent");
	ASSERT_EQ(buf[0], '%', "percent2");

	len = snprintf(buf, sizeof(buf), "%%");
	ASSERT_EQ(len, 1, "double percent");
	ASSERT_EQ(buf[0], '%', "dpercent2");

	len = snprintf(buf, sizeof(buf), "%u", 100);
	ASSERT_EQ(len, 3, "100");
	len = snprintf(buf, sizeof(buf), "%X", 10);
	ASSERT_EQ(len, 1, "a");
	ASSERT_EQ(buf[0], 'A', "abuf");

	len = snprintf(buf, sizeof(buf), "%s", NULL);
	ASSERT_EQ(len, (int)strlen("(null)"), "null");

	len = snprintf(buf, sizeof(buf), "%v");
	ASSERT_EQ(len, 1, "len=2");
	ASSERT_EQ(buf[0], 'v', "buf[0]=v");
}

Test(b64) {
	uint8_t buf[128];
	uint8_t buf2[128];
	uint8_t buf3[128];
	int len, len2;
	memcpy(buf, "0123456789", 10);
	len = b64_encode(buf, 10, buf2, 128);
	len2 = b64_decode(buf2, len, buf3, 128);
	ASSERT_EQ(len2, 10, "len=10");
	ASSERT_EQ(buf3[0], '0', "0");
	ASSERT_EQ(buf3[1], '1', "1");
	ASSERT_EQ(buf3[2], '2', "2");
	ASSERT_EQ(buf3[3], '3', "3");
	ASSERT_EQ(buf3[4], '4', "4");
	ASSERT_EQ(buf3[5], '5', "5");
	ASSERT_EQ(buf3[6], '6', "6");
	ASSERT_EQ(buf3[7], '7', "7");
	ASSERT_EQ(buf3[8], '8', "8");
	ASSERT_EQ(buf3[9], '9', "9");
}

/* Helper function to compare encoded output with expected string */
static void assert_b64_eq(const uint8_t *out, const char *expected,
			  const char *msg) {
	ASSERT(!strcmp((const char *)out, expected), msg);
}

Test(b642) {
	uint8_t buf[128];
	uint8_t buf2[128];
	uint8_t buf3[128];
	uint64_t len, len2;

	/* Test 1: Normal case (10 bytes, exercises main loop) */
	memcpy(buf, "0123456789", 10);
	len = b64_encode(buf, 10, buf2, 128);
	ASSERT_EQ(len, 16, "normal_len"); /* (10+2)/3 * 4 = 16 */
	assert_b64_eq(buf2, "MDEyMzQ1Njc4OQ==", "normal_encode");
	len2 = b64_decode(buf2, len, buf3, 128);
	ASSERT_EQ(len2, 10, "normal_decode_len");
	ASSERT(!memcmp(buf3, "0123456789", 10), "normal_decode");

	/* Test 2: Two-byte input (covers lines 601–605) */
	memcpy(buf, "ab", 2);
	len = b64_encode(buf, 2, buf2, 128);
	ASSERT_EQ(len, 4, "two_byte_len"); /* (2+2)/3 * 4 = 4 */
	assert_b64_eq(buf2, "YWI=", "two_byte_encode");
	len2 = b64_decode(buf2, len, buf3, 128);
	ASSERT_EQ(len2, 2, "two_byte_decode_len");
	ASSERT(!memcmp(buf3, "ab", 2), "two_byte_decode");

	/* Test 3: Single-byte input (covers lines 607–610) */
	memcpy(buf, "x", 1);
	len = b64_encode(buf, 1, buf2, 128);
	ASSERT_EQ(len, 4, "single_byte_len"); /* (1+2)/3 * 4 = 4 */
	assert_b64_eq(buf2, "eA==", "single_byte_encode");
	len2 = b64_decode(buf2, len, buf3, 128);
	ASSERT_EQ(len2, 1, "single_byte_decode_len");
	ASSERT(!memcmp(buf3, "x", 1), "single_byte_decode");

	/* Test 4: Empty input */
	len = b64_encode(buf, 0, buf2, 128);
	ASSERT_EQ(len, 0, "empty_len");
	ASSERT_EQ(buf2[0], 0, "empty_no_write"); /* Ensure output unchanged */

	/* Test 5: Null input */
	len = b64_encode(NULL, 5, buf2, 128);
	ASSERT_EQ(len, 0, "null_in_len");
	len = b64_encode(buf, 5, NULL, 128);
	ASSERT_EQ(len, 0, "null_out_len");

	/* Test 6: Insufficient output buffer */
	memcpy(buf, "abc", 3);
	len = b64_encode(buf, 3, buf2,
			 4); /* Needs 4 bytes + 1 for null, 4 is too small */
	ASSERT_EQ(len, 0, "insufficient_out_len");
	ASSERT_EQ(buf2[0], 0, "insufficient_no_write");

	/* Test 7: Five-byte input (main loop + two-byte remainder) */
	memcpy(buf, "abcde", 5);
	len = b64_encode(buf, 5, buf2, 128);
	ASSERT_EQ(len, 8, "five_byte_len"); /* (5+2)/3 * 4 = 8 */
	assert_b64_eq(buf2, "YWJjZGU=", "five_byte_encode");
	len2 = b64_decode(buf2, len, buf3, 128);
	ASSERT_EQ(len2, 5, "five_byte_decode_len");
	ASSERT(!memcmp(buf3, "abcde", 5), "five_byte_decode");
}

extern int cur_tasks;
int ecount = 0;
void my_exit(void) { ecount++; }

Test(begin) {
	int i;
	cur_tasks = 1;
	begin();
	ASSERT_EQ(cur_tasks, 0, "cur_tasks");
	for (i = 0; i < 64; i++)
		ASSERT(!register_exit(my_exit), "register_exit");
	for (i = 0; i < 3; i++)
		ASSERT(register_exit(my_exit), "register_exit_overflow");

	execute_exits();
	ASSERT_EQ(ecount, 64, "64 exits");
}

Test(pipe) {
	int fds[2], fv;
	err = 0;
	ASSERT(!pipe(fds), "pipe");
	ASSERT(!err, "not err");
	if ((fv = two())) {
		char buf[10];
		int v = read(fds[0], buf, 10);
		ASSERT_EQ(v, 4, "v==4");
		ASSERT_EQ(buf[0], 't', "t");
		ASSERT_EQ(buf[1], 'e', "e");
		ASSERT_EQ(buf[2], 's', "s");
		ASSERT_EQ(buf[3], 't', "t");

	} else {
		write(fds[1], "test", 4);
		exit(0);
	}
	waitid(P_PID, fv, NULL, WEXITED);
}

Test(files1) {
	const char *fname = "/tmp/ftest.tmp";
	int dup, fd;
	unlink(fname);
	err = 0;
	fd = file(fname);
	ASSERT(fd > 0, "fd > 0");

	ASSERT(exists(fname), "exists");

	err = 0;
	ASSERT(!fsize(fd), "fsize");
	write(fd, "abc", 3);
	flush(fd);
	ASSERT_EQ(fsize(fd), 3, "fsize==3");

	fresize(fd, 2);
	ASSERT_EQ(fsize(fd), 2, "fisze==2");
	dup = fcntl(fd, F_DUPFD);
	ASSERT(dup != fd, "dup");

	ASSERT(!unlink(fname), "unlink");
	ASSERT(!exists(fname), "exists");
	close(fd);
	close(dup);
}

Test(entropy) {
	uint8_t buf[1024];
	uint128_t v1 = 0, v2 = 0;
	ASSERT(!getentropy(buf, 64), "getentropy");
	ASSERT(getentropy(buf, 1024), "overflow");
	ASSERT(getentropy(NULL, 100), "null");
	ASSERT_EQ(v1, v2, "eq");
	getentropy(&v1, sizeof(uint128_t));
	getentropy(&v2, sizeof(uint128_t));
	ASSERT(v1 != v2, "v1 != v2");
	ASSERT(v1 != 0, "v1 != 0");
	ASSERT(v2 != 0, "v2 != 0");
}

Test(map_err) {
	char buf[100];
	void *v;
	err = 0;
	v = mmap(buf, 100, PROT_READ | PROT_WRITE, MAP_SHARED, -1, 0);
	ASSERT((int64_t)v == -1, "mmap err");
	ASSERT_EQ(err, EBADF, "ebadf");
	v = map(PAGE_SIZE);
	ASSERT(v, "v != null");
	ASSERT(!munmap(v, PAGE_SIZE), "munmap");
	err = 0;
	ASSERT(!map(SIZE_MAX), "size_max");
	ASSERT_EQ(err, ENOMEM, "enomem");
	ASSERT(fmap(1, SIZE_MAX, SIZE_MAX) == NULL, "fmap err");
	yield();
	ASSERT(smap(SIZE_MAX) == NULL, "smap err");
	ASSERT(settimeofday(NULL, NULL), "settimeofday err");
}

Test(memcmp) {
	char buf1[11] = "abcde12345";
	char buf2[11] = "abcde12345"; /* Identical to buf1 */
	char buf3[11] = "abcde12346"; /* Differs at last byte ('6' vs '5') */
	char buf4[11] = "abcde12344"; /* Differs at last byte ('4' vs '5') */
	char buf5[11] = "abcda12345"; /* Differs at 5th byte ('a' vs 'e') */

	/* Test 1: Equal buffers (full length) */
	ASSERT_EQ(memcmp(buf1, buf2, 10), 0, "equal_full_length");

	/* Test 2: Buffers differ, s1 < s2 (hits line 146, negative return) */
	ASSERT(memcmp(buf1, buf3, 10) < 0,
	       "less_than"); /* '5' < '6' at position 9 */

	/* Test 3: Buffers differ, s1 > s2 (hits line 146, positive return) */
	ASSERT(memcmp(buf1, buf4, 10) > 0,
	       "greater_than"); /* '5' > '4' at position 9 */

	/* Test 4: Partial equality (equal up to 9 bytes, then differ) */
	ASSERT_EQ(memcmp(buf1, buf3, 9), 0,
		  "partial_equal"); /* Equal up to '3' */

	/* Test 5: Zero length comparison */
	ASSERT_EQ(memcmp(buf1, buf3, 0), 0,
		  "zero_length"); /* No bytes compared */

	/* Test 6: Single byte, s1 < s2 */
	ASSERT(memcmp("a", "b", 1) < 0, "single_byte_less"); /* 'a' < 'b' */

	/* Test 7: Single byte, s1 > s2 */
	ASSERT(memcmp("b", "a", 1) > 0, "single_byte_greater"); /* 'b' > 'a' */

	/* Test 8: Early difference (hits line 146, positive return) */
	ASSERT(memcmp(buf1, buf5, 10) > 0,
	       "early_difference"); /* 'e' > 'a' at position 4 */
}
