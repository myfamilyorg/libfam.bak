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

#include <libfam/alloc.H>
#include <libfam/atomic.H>
#include <libfam/env.H>
#include <libfam/error.H>
#include <libfam/format.H>
#include <libfam/init.H>
#include <libfam/syscall_const.H>
#include <libfam/test.H>

typedef struct {
	i32 value1;
	i32 value2;
	i32 value3;
	i32 value4;
	i32 value5;
	u32 uvalue1;
	u32 uvalue2;
} SharedStateData;

Test(alloc1) {
	u8 *t1, *t2, *t3, *t4, *t5, *t6, *t7;
	Alloc *a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	ASSERT(a, "a!=NULL");
	t1 = alloc_impl(a, CHUNK_SIZE);
	t2 = alloc_impl(a, CHUNK_SIZE);
	t3 = alloc_impl(a, CHUNK_SIZE);
	t4 = alloc_impl(a, CHUNK_SIZE);
	t5 = alloc_impl(a, CHUNK_SIZE);

	ASSERT(t1, "t1!=NULL");
	ASSERT(t2, "t2!=NULL");
	ASSERT(t3, "t3!=NULL");
	ASSERT(t4, "t4!=NULL");
	ASSERT(t5, "t5!=NULL");

	ASSERT(t1 != t2, "t1 != t2");
	ASSERT(t2 != t3, "t2 != t3");
	ASSERT(t3 != t4, "t3 != t4");
	ASSERT(t4 != t5, "t4 != t5");

	release_impl(a, t3);
	t6 = alloc_impl(a, CHUNK_SIZE);
	ASSERT_EQ(t3, t6, "last_free");

	t7 = alloc_impl(a, CHUNK_SIZE);
	ASSERT((u64)t7 > (u64)t6, "t7>t6");
	ASSERT((u64)t7 > (u64)t5, "t7>t5");

	release_impl(a, t1);
	release_impl(a, t2);
	release_impl(a, t4);
	release_impl(a, t5);
	release_impl(a, t6);
	release_impl(a, t7);

	_debug_alloc_failure = 2;
	ASSERT(!alloc(1), "fail1");
	ASSERT(!alloc(1), "fail1");
	t1 = alloc(1);
	ASSERT(t1, "success");
	release(t1);

	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(alloc2) {
	Alloc *a;
	u8 *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL,
	   *t6 = NULL, *t7 = NULL;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);

	t1 = alloc_impl(a, CHUNK_SIZE / 4);

	t2 = alloc_impl(a, CHUNK_SIZE / 4);
	ASSERT_EQ((u64)t2 - (u64)t1, CHUNK_SIZE / 4, "diff=1mb");

	t3 = alloc_impl(a, CHUNK_SIZE / 4);
	ASSERT_EQ((u64)t3 - (u64)t2, CHUNK_SIZE / 4, "diff=1mb");

	t4 = alloc_impl(a, CHUNK_SIZE / 4);
	/* only 3 slabs fit in a chunk due to overhead */
	ASSERT((u64)t4 - (u64)t3 != CHUNK_SIZE / 4, "diff!=1mb");

	t5 = alloc_impl(a, CHUNK_SIZE / 4);
	ASSERT_EQ((u64)t5 - (u64)t4, CHUNK_SIZE / 4, "diff=1mb");

	t6 = alloc_impl(a, CHUNK_SIZE / 4);
	ASSERT_EQ((u64)t6 - (u64)t5, CHUNK_SIZE / 4, "diff=1mb");

	t7 = alloc_impl(a, CHUNK_SIZE / 4);
	/* only 3 slabs fit in a chunk due to overhead */
	ASSERT((u64)t7 - (u64)t6 != CHUNK_SIZE / 4, "diff!=1mb");

	release_impl(a, t1);
	release_impl(a, t2);
	release_impl(a, t3);
	release_impl(a, t4);
	release_impl(a, t5);
	release_impl(a, t6);
	release_impl(a, t7);

	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(alloc3) {
	Alloc *a;
	u8 *t1 = NULL, *t2 = NULL;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);

	t1 = alloc_impl(a, CHUNK_SIZE + 1);
	t2 = alloc_impl(a, CHUNK_SIZE + 1);
	ASSERT(t1, "t1!=NULL");
	ASSERT(t2, "t2!=NULL");
	ASSERT_EQ((u64)t2 - (u64)t1, CHUNK_SIZE * 2, "2 chunks required");
	release_impl(a, t1);
	release_impl(a, t2);

	t1 = alloc_impl(a, 2 * CHUNK_SIZE - 16);
	t2 = alloc_impl(a, 2 * CHUNK_SIZE - 16);
	ASSERT(t1, "t1!=NULL");
	ASSERT(t2, "t2!=NULL");
	ASSERT_EQ((u64)t2 - (u64)t1, CHUNK_SIZE * 2, "2 chunks still required");
	release_impl(a, t1);
	release_impl(a, t2);

	t1 = alloc_impl(a, 2 * CHUNK_SIZE - 15);
	t2 = alloc_impl(a, 2 * CHUNK_SIZE - 15);
	ASSERT(t1, "t1!=NULL");
	ASSERT(t2, "t2!=NULL");
	ASSERT_EQ((u64)t2 - (u64)t1, CHUNK_SIZE * 3, "3 chunks required");
	release_impl(a, t1);
	release_impl(a, t2);

	t1 = alloc_impl(a, CHUNK_SIZE * 16);
	ASSERT(!t1, "t1=NULL");
	t1 = alloc_impl(a, CHUNK_SIZE * 16 - 16);
	ASSERT(t1, "t1!=NULL");
	release_impl(a, t1);

	/* Test panic dealloc */
	_debug_no_exit = true;
	_debug_no_write = true;
	release_impl(a, (void *)1);
	_debug_no_write = false;
	_debug_no_exit = false;

	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(resize1) {
	Alloc *a;
	u8 *t1 = NULL, *t2 = NULL, *t3 = NULL;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);

	t1 = resize_impl(a, t1, 8);
	t2 = resize_impl(a, t2, 8);
	t3 = resize_impl(a, t3, 14);

	ASSERT_EQ((u64)t2 - (u64)t1, 8, "8 byte slabs");

	release_impl(a, t1);
	resize_impl(a, t2, 0);
	resize_impl(a, t3, 0);

	t1 = resize_impl(a, NULL, CHUNK_SIZE);
	ASSERT(t1, "t1!=NULL");
#if MEMSAN == 1
	ASSERT_EQ(allocated_bytes_impl(a), CHUNK_SIZE, "alloc=CHUNK_SIZE");
#endif

	t1 = resize_impl(a, t1, CHUNK_SIZE + 18);
#if MEMSAN == 1
	ASSERT_EQ(allocated_bytes_impl(a), 2 * CHUNK_SIZE,
		  "alloc=2* CHUNK_SIZE");
#endif

	ASSERT(t1, "t1!=NULL");

	t1 = resize_impl(a, t1, 8);
	ASSERT(t1, "t1!=NULL");
#if MEMSAN == 1
	ASSERT_EQ(allocated_bytes_impl(a), 8, "alloc=8");
#endif

	release_impl(a, t1);

	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(calloc1) {
	void *ptr1 = calloc(100, 10);
	ASSERT(ptr1, "ptr1!=NULL");
	release(ptr1);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Walloc-size-larger-than="
	ptr1 = calloc(I64_MAX - 1, I64_MAX - 1);
#pragma GCC pop
	ASSERT(!ptr1, "ptr1==NULL");

	ASSERT_BYTES(0);
}

Test(enomem) {
	Alloc *a;
	u8 *t1 = NULL;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE);
	t1 = alloc_impl(a, CHUNK_SIZE);
	ASSERT(t1, "t1 != NULL");
	err = 0;
	ASSERT(!alloc_impl(a, CHUNK_SIZE), "enomem");
	ASSERT_EQ(err, ENOMEM, "err");
	release_impl(a, t1);
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
	ASSERT_EQ(get_allocated_bytes(), 0, "alloc=0");

	_debug_no_exit = true;
	_debug_no_write = true;
	release_impl(NULL, NULL);
	_debug_no_write = false;
	_debug_no_exit = false;
}

u64 get_memory_bytes(void);

Test(get_memory_bytes) {
	void *ptr;
	setenv("SHARED_MEMORY_BYTES", "10", true);
	_debug_no_write = true;
	ASSERT(get_memory_bytes() != 10, "invalid env value");
	_debug_no_write = false;
	unsetenv("SHARED_MEMORY_BYTES");
	err = 0;
	ASSERT_EQ(alloc_init(100, CHUNK_SIZE * 16), NULL, "invalid type");
	ASSERT_EQ(err, EINVAL, "err");
	ASSERT_EQ(alloc(CHUNK_SIZE * 64), NULL, "too big");
	ptr = alloc(8);
	ASSERT_EQ(resize(ptr, CHUNK_SIZE * 64), NULL, "too big");
	release(ptr);
}

Test(fmap) {
	const u8 *path = "/tmp/fmap.dat";
	i32 fd;
	Alloc *a;

	unlink(path);
	fd = file(path);
	fresize(fd, CHUNK_SIZE * 17);
	a = alloc_init(ALLOC_TYPE_FMAP, CHUNK_SIZE * 16, fd);
	ASSERT(a, "a!=NULL");

	alloc_destroy(a);
	close(fd);
	unlink(path);
}

Test(resize2) {
	void *t1 = alloc(CHUNK_SIZE);
	ASSERT(t1, "t1!=NULL");
	t1 = resize(t1, CHUNK_SIZE - 1);
	ASSERT(t1, "t1!=NULL");
	t1 = resize(t1, 8);
	ASSERT(t1, "t1!=NULL");
	t1 = resize(t1, 16);
	ASSERT(t1, "t1!=NULL");
	t1 = resize(t1, CHUNK_SIZE);
	ASSERT(t1, "t1!=NULL");

	_debug_no_exit = true;
	_debug_no_write = true;
	ASSERT(!resize((void *)1, 128), "invalid pointer");
	_debug_no_exit = false;
	_debug_no_write = false;

	release(t1);

	ASSERT_BYTES(0);
}

Test(cas_loop) {
	Alloc *a;
	u8 *t1 = NULL;

	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE);

	_debug_cas_loop = 1;
	t1 = alloc_impl(a, 1024);
	ASSERT(t1, "t1 != NULL");
	ASSERT_EQ(_debug_cas_loop, 0, "_debug_cas_loop");
}

Test(format1) {
	i32 x = 101;
	Formatter f = {0};
	format(&f, "test1={},test2={},test3={}", 1, "abc", -100);
	ASSERT(!strcmp(format_to_string(&f), "test1=1,test2=abc,test3=-100"),
	       "strcmp");
	format_clear(&f);

	format(&f, "test1={},test2={},test3={}x", 1, "abc", -100);
	ASSERT(!strcmp(format_to_string(&f), "test1=1,test2=abc,test3=-100x"),
	       "strcmp2");

	format_clear(&f);
	format(&f, "abc");
	ASSERT(!strcmp(format_to_string(&f), "abc"), "strcmp2");

	format_clear(&f);
	format(&f, "abc={}", x);
	ASSERT(!strcmp(format_to_string(&f), "abc=101"), "strcmp3");

	format_clear(&f);

	format(&f, "abc={}", (u128)107);
	ASSERT(!strcmp(format_to_string(&f), "abc=107"), "strcmp4");
	format_clear(&f);

	ASSERT(strcmp(NULL, "abc"), "strcmp with NULL");
	ASSERT_EQ(memcmp("abc", "abd", 3), -1, "memcmp");

	ASSERT_BYTES(0);
}

Test(atomic) {
	u32 x = 2, a32 = 0, b32 = 1;
	u64 y = 2, a64 = 0, b64 = 1;

	__or32(&x, 1);
	ASSERT_EQ(x, 3, "or32");

	__or64(&y, 1);
	ASSERT_EQ(y, 3, "or64");

	__sub64(&y, 1);
	ASSERT_EQ(y, 2, "sub64");

	ASSERT(!__cas32(&a32, &b32, 0), "cas32");

	__sub32(&x, 1);
	ASSERT_EQ(x, 2, "x=2");
	__and32(&x, 3);
	ASSERT_EQ(x, 2, "x=2and");

	__and64(&y, 1);
	ASSERT(!y, "!y");

	ASSERT(!__cas64(&a64, &b64, 0), "cas64");
	ASSERT_BYTES(0);
}

Test(pipe) {
	i32 fds[2], fv;
	err = 0;
	ASSERT(!pipe(fds), "pipe");
	ASSERT(!err, "not err");
	if ((fv = two())) {
		u8 buf[10];
		i32 v = read(fds[0], buf, 10);
		ASSERT_EQ(v, 4, "v==4");
		ASSERT_EQ(buf[0], 't', "t");
		ASSERT_EQ(buf[1], 'e', "e");
		ASSERT_EQ(buf[2], 's', "s");
		ASSERT_EQ(buf[3], 't', "t");

	} else {
		write(fds[1], "test", 4);
		exit(0);
	}
	fv = waitid(P_PID, fv, NULL, WEXITED);

	close(fds[0]);
	close(fds[1]);
	ASSERT_BYTES(0);
}

Test(colors) {
	const u8 *test_file = "/tmp/colortest";
	i32 fd;

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
	ASSERT_BYTES(0);
}

Test(strdup) {
	const u8 *t1 = "test1";
	u8 *res = strdup(t1);
	ASSERT(res, "res!=MULL");
	ASSERT(res != t1, "res != t1");
	ASSERT(!strcmp(t1, res), "strcmp t1 == res");
	release(res);
	ASSERT_BYTES(0);
}

Test(env) {
	ASSERT(getenv("TEST_PATTERN"), "getenv1");
	ASSERT(!getenv("__TEST_PATTERNS__"), "getenv2");
	setenv("__TEST_PATTERNS__", "1", true);
	ASSERT(getenv("__TEST_PATTERNS__"), "getenv3");
	setenv("__TEST_PATTERNS__", "2", true);
	ASSERT(!strcmp(getenv("__TEST_PATTERNS__"), "2"), "tp=2");
	unsetenv("__TEST_PATTERNS__");
	ASSERT(!getenv("__TEST_PATTERNS__"), "getenv4");

	_debug_alloc_failure_bypass_count = 1;
	_debug_alloc_failure = 1;
	ASSERT_EQ(setenv("__TEST_PATTERNS__", "3", true), -1,
		  "alloc failure err");

	_debug_alloc_failure_bypass_count = 2;
	_debug_alloc_failure = 1;
	init_environ();
	ASSERT_EQ(_debug_alloc_failure, 0, "alloc_failure_reset");
	ASSERT_EQ(_debug_alloc_failure_bypass_count, 0,
		  "_debug_alloc_failure_bypass_count");
	ASSERT_BYTES(0);
}

i32 *__error(void);

Test(errors) {
	ASSERT(!strcmp("Success", strerror(0)), "success");
	ASSERT(!strcmp("Operation not permitted", strerror(EPERM)), "eperm");
	ASSERT(!strcmp("No such file or directory", strerror(ENOENT)),
	       "enoent");
	ASSERT(!strcmp("No such process", strerror(ESRCH)), "esrch");
	ASSERT(!strcmp("Interrupted system call", strerror(EINTR)), "eintr");
	ASSERT(!strcmp("Input/output error", strerror(EIO)), "eio");
	ASSERT(!strcmp("No such device or address", strerror(ENXIO)), "enxio");
	ASSERT(!strcmp("Argument list too long", strerror(E2BIG)), "e2big");
	ASSERT(!strcmp("Exec format error", strerror(ENOEXEC)), "enoexec");
	ASSERT(!strcmp("Bad file descriptor", strerror(EBADF)), "ebadf");
	ASSERT(!strcmp("No child processes", strerror(ECHILD)), "echild");
	ASSERT(!strcmp("Resource temporarily unavailable", strerror(EAGAIN)),
	       "eagain");
	ASSERT(!strcmp("Out of memory", strerror(ENOMEM)), "enomem");
	ASSERT(!strcmp("Permission denied", strerror(EACCES)), "eacces");
	ASSERT(!strcmp("Bad address", strerror(EFAULT)), "efault");
	ASSERT(!strcmp("Block device required", strerror(ENOTBLK)), "enotblk");
	ASSERT(!strcmp("Device or resource busy", strerror(EBUSY)), "ebusy");
	ASSERT(!strcmp("File exists", strerror(EEXIST)), "eexist");
	ASSERT(!strcmp("Invalid cross-device link", strerror(EXDEV)), "exdev");
	ASSERT(!strcmp("No such device", strerror(ENODEV)), "enodev");
	ASSERT(!strcmp("Not a directory", strerror(ENOTDIR)), "enotdir");
	ASSERT(!strcmp("Is a directory", strerror(EISDIR)), "eisdir");
	ASSERT(!strcmp("Invalid argument", strerror(EINVAL)), "einval");
	ASSERT(!strcmp("File table overflow", strerror(ENFILE)), "enfile");
	ASSERT(!strcmp("Too many open files", strerror(EMFILE)), "emfile");
	ASSERT(!strcmp("Not a typewriter", strerror(ENOTTY)), "enotty");
	ASSERT(!strcmp("Text file busy", strerror(ETXTBSY)), "etxtbsy");
	ASSERT(!strcmp("File too large", strerror(EFBIG)), "efbig");
	ASSERT(!strcmp("No space left on device", strerror(ENOSPC)), "enospc");
	ASSERT(!strcmp("Illegal seek", strerror(ESPIPE)), "espipe");
	ASSERT(!strcmp("Read-only file system", strerror(EROFS)), "erofs");
	ASSERT(!strcmp("Too many links", strerror(EMLINK)), "emlink");
	ASSERT(!strcmp("Broken pipe", strerror(EPIPE)), "epipe");
	ASSERT(!strcmp("Math argument out of domain of func", strerror(EDOM)),
	       "edom");
	ASSERT(!strcmp("Math result not representable", strerror(ERANGE)),
	       "erange");
	ASSERT(!strcmp("Resource deadlock would occur", strerror(EDEADLK)),
	       "edeadlk");
	ASSERT(!strcmp("File name too long", strerror(ENAMETOOLONG)),
	       "enametoolong");
	ASSERT(!strcmp("No record locks available", strerror(ENOLCK)),
	       "enolck");
	ASSERT(!strcmp("Invalid system call number", strerror(ENOSYS)),
	       "enosys");
	ASSERT(!strcmp("Directory not empty", strerror(ENOTEMPTY)),
	       "enotempty");
	ASSERT(!strcmp("Too many symbolic links encountered", strerror(ELOOP)),
	       "eloop");
	ASSERT(!strcmp("No message of desired type", strerror(ENOMSG)),
	       "enomsg");
	ASSERT(!strcmp("Identifier removed", strerror(EIDRM)), "eidrm");
	ASSERT(!strcmp("Channel number out of range", strerror(ECHRNG)),
	       "echrng");
	ASSERT(!strcmp("Level 2 not synchronized", strerror(EL2NSYNC)),
	       "el2nsync");
	ASSERT(!strcmp("Level 3 halted", strerror(EL3HLT)), "el3hlt");
	ASSERT(!strcmp("Level 3 reset", strerror(EL3RST)), "el3rst");
	ASSERT(!strcmp("Link number out of range", strerror(ELNRNG)), "elnrng");
	ASSERT(!strcmp("Protocol driver not attached", strerror(EUNATCH)),
	       "eunatch");
	ASSERT(!strcmp("No CSI structure available", strerror(ENOCSI)),
	       "enocsi");
	ASSERT(!strcmp("Level 2 halted", strerror(EL2HLT)), "el2hlt");
	ASSERT(!strcmp("Invalid exchange", strerror(EBADE)), "ebade");
	ASSERT(!strcmp("Invalid request descriptor", strerror(EBADR)), "ebadr");
	ASSERT(!strcmp("Exchange full", strerror(EXFULL)), "exfull");
	ASSERT(!strcmp("No anode", strerror(ENOANO)), "enoano");
	ASSERT(!strcmp("Invalid request code", strerror(EBADRQC)), "ebadrqc");
	ASSERT(!strcmp("Invalid slot", strerror(EBADSLT)), "ebadslt");
	ASSERT(!strcmp("Bad font file format", strerror(EBFONT)), "ebfont");
	ASSERT(!strcmp("Device not a stream", strerror(ENOSTR)), "enostr");
	ASSERT(!strcmp("No data available", strerror(ENODATA)), "enodata");
	ASSERT(!strcmp("Timer expired", strerror(ETIME)), "etime");
	ASSERT(!strcmp("Out of streams resources", strerror(ENOSR)), "enosr");
	ASSERT(!strcmp("Machine is not on the network", strerror(ENONET)),
	       "enonet");
	ASSERT(!strcmp("Package not installed", strerror(ENOPKG)), "enopkg");
	ASSERT(!strcmp("Object is remote", strerror(EREMOTE)), "eremote");
	ASSERT(!strcmp("Link has been severed", strerror(ENOLINK)), "enolink");
	ASSERT(!strcmp("Advertise error", strerror(EADV)), "eadv");
	ASSERT(!strcmp("Srmount error", strerror(ESRMNT)), "esrmnt");
	ASSERT(!strcmp("Communication error on send", strerror(ECOMM)),
	       "ecomm");
	ASSERT(!strcmp("Protocol error", strerror(EPROTO)), "eproto");
	ASSERT(!strcmp("Multihop attempted", strerror(EMULTIHOP)), "emultihop");

	ASSERT(!strcmp("RFS specific error", strerror(EDOTDOT)), "edotdot");
	ASSERT(!strcmp("Not a data message", strerror(EBADMSG)), "ebadmsg");
	ASSERT(!strcmp("Value too large for defined data type",
		       strerror(EOVERFLOW)),
	       "eoverflow");
	ASSERT(!strcmp("Name not unique on network", strerror(ENOTUNIQ)),
	       "enotuniq");
	ASSERT(!strcmp("File descriptor in bad state", strerror(EBADFD)),
	       "ebadfd");
	ASSERT(!strcmp("Remote address changed", strerror(EREMCHG)), "eremchg");
	ASSERT(!strcmp("Can not access a needed shared library",
		       strerror(ELIBACC)),
	       "elibacc");
	ASSERT(
	    !strcmp("Accessing a corrupted shared library", strerror(ELIBBAD)),
	    "elibbad");
	ASSERT(!strcmp("lib section in a.out corrupted", strerror(ELIBSCN)),
	       "elibscn");
	ASSERT(!strcmp("Attempting to link in too many shared libraries",
		       strerror(ELIBMAX)),
	       "elibmax");
	ASSERT(!strcmp("Cannot exec a shared library directly",
		       strerror(ELIBEXEC)),
	       "elibexec");
	ASSERT(!strcmp("Illegal byte sequence", strerror(EILSEQ)), "eilseq");
	ASSERT(!strcmp("Interrupted system call should be restarted",
		       strerror(ERESTART)),
	       "erestart");
	ASSERT(!strcmp("Streams pipe error", strerror(ESTRPIPE)), "estrpipe");
	ASSERT(!strcmp("Too many users", strerror(EUSERS)), "eusers");
	ASSERT(!strcmp("Socket operation on non-socket", strerror(ENOTSOCK)),
	       "enotsock");
	ASSERT(!strcmp("Destination address required", strerror(EDESTADDRREQ)),
	       "edestaddrreq");
	ASSERT(!strcmp("Message too long", strerror(EMSGSIZE)), "emsgsize");
	ASSERT(!strcmp("Protocol wrong type for socket", strerror(EPROTOTYPE)),
	       "eprototype");

	ASSERT(!strcmp("Protocol not available", strerror(ENOPROTOOPT)),
	       "enoprotoopt");
	ASSERT(!strcmp("Protocol not supported", strerror(EPROTONOSUPPORT)),
	       "eprotonosupport");
	ASSERT(!strcmp("Socket type not supported", strerror(ESOCKTNOSUPPORT)),
	       "esocktnosupport");
	ASSERT(!strcmp("Operation not supported on transport endpoint",
		       strerror(EOPNOTSUPP)),
	       "eopnotsupp");
	ASSERT(!strcmp("Protocol family not supported", strerror(EPFNOSUPPORT)),
	       "epfnosupport");
	ASSERT(!strcmp("Address family not supported by protocol",
		       strerror(EAFNOSUPPORT)),
	       "eafnosupport");
	ASSERT(!strcmp("Address already in use", strerror(EADDRINUSE)),
	       "eaddrinuse");
	ASSERT(
	    !strcmp("Cannot assign requested address", strerror(EADDRNOTAVAIL)),
	    "eaddrnotavail");
	ASSERT(!strcmp("Network is down", strerror(ENETDOWN)), "enetdown");
	ASSERT(!strcmp("Network is unreachable", strerror(ENETUNREACH)),
	       "enetunreach");
	ASSERT(!strcmp("Network dropped connection because of reset",
		       strerror(ENETRESET)),
	       "enetreset");
	ASSERT(
	    !strcmp("Software caused connection abort", strerror(ECONNABORTED)),
	    "econnaborted");
	ASSERT(!strcmp("Connection reset by peer", strerror(ECONNRESET)),
	       "econnreset");
	ASSERT(!strcmp("No buffer space available", strerror(ENOBUFS)),
	       "enobufs");
	ASSERT(!strcmp("Transport endpoint is already connected",
		       strerror(EISCONN)),
	       "eisconn");
	ASSERT(
	    !strcmp("Transport endpoint is not connected", strerror(ENOTCONN)),
	    "enotconn");
	ASSERT(!strcmp("Cannot send after transport endpoint shutdown",
		       strerror(ESHUTDOWN)),
	       "eshutdown");

	ASSERT(!strcmp("Too many references: cannot splice",
		       strerror(ETOOMANYREFS)),
	       "etoomanyrefs");
	ASSERT(!strcmp("Connection timed out", strerror(ETIMEDOUT)),
	       "etimedout");
	ASSERT(!strcmp("Connection refused", strerror(ECONNREFUSED)),
	       "econnrefused");
	ASSERT(!strcmp("Host is down", strerror(EHOSTDOWN)), "ehostdown");
	ASSERT(!strcmp("No route to host", strerror(EHOSTUNREACH)),
	       "ehostunreach");
	ASSERT(!strcmp("Operation already in progress", strerror(EALREADY)),
	       "ealready");
	ASSERT(!strcmp("Operation now in progress", strerror(EINPROGRESS)),
	       "einprogress");
	ASSERT(!strcmp("Stale file handle", strerror(ESTALE)), "estale");
	ASSERT(!strcmp("Structure needs cleaning", strerror(EUCLEAN)),
	       "euclean");
	ASSERT(!strcmp("Not a XENIX named type file", strerror(ENOTNAM)),
	       "enotnam");
	ASSERT(!strcmp("No XENIX semaphores available", strerror(ENAVAIL)),
	       "enavail");
	ASSERT(!strcmp("Is a named type file", strerror(EISNAM)), "eisnam");
	ASSERT(!strcmp("Remote I/O error", strerror(EREMOTEIO)), "eremoteio");
	ASSERT(!strcmp("Quota exceeded", strerror(EDQUOT)), "edquot");
	ASSERT(!strcmp("No medium found", strerror(ENOMEDIUM)), "enomedium");
	ASSERT(!strcmp("Wrong medium type", strerror(EMEDIUMTYPE)),
	       "emediumtype");
	ASSERT(!strcmp("Operation canceled", strerror(ECANCELED)), "ecanceled");
	ASSERT(!strcmp("Required key not available", strerror(ENOKEY)),
	       "enokey");
	ASSERT(!strcmp("Key has expired", strerror(EKEYEXPIRED)),
	       "ekeyexpired");
	ASSERT(!strcmp("Key has been revoked", strerror(EKEYREVOKED)),
	       "ekeyrevoked");
	ASSERT(!strcmp("Key was rejected by service", strerror(EKEYREJECTED)),
	       "ekeyrejected");

	ASSERT(!strcmp("Unknown error", strerror(1000)), "unknown");

	err = 0;

	ASSERT_EQ(*__error(), 0, "__error");
	_debug_no_write = true;
	perror("test");
	_debug_no_write = false;
	ASSERT_BYTES(0);
}

extern i32 cur_tasks, has_begun;
i32 ecount = 0;
void my_exit(void) { ecount++; }

Test(begin) {
	i32 i;
	cur_tasks = 1;
	begin();
	ASSERT_EQ(cur_tasks, 0, "cur_tasks");
	for (i = 0; i < 64; i++)
		ASSERT(!register_exit(my_exit), "register_exit");
	for (i = 0; i < 3; i++)
		ASSERT(register_exit(my_exit), "register_exit_overflow");

	execute_exits();
	ASSERT_EQ(ecount, 64, "64 exits");
	/* Reset has_begun for other tests */
	has_begun = 0;
}

u32 tfunv1 = 0;
u32 tfunv2 = 0;
u32 tfunv3 = 0;

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
	i32 pid, i;
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
		for (i = 0; i < 3; i++) sleep(200);
		ASSERT_EQ(ALOAD(&tfunv1), 1, "v1 1");
		ASSERT_EQ(ALOAD(&tfunv2), 1, "v2 1");
		ASSERT_EQ(ALOAD(&tfunv3), 1, "v3 1");
		while (!ALOAD(&state->value1));
	} else {
		timeout(tfun3, 150);
		for (i = 0; i < 3; i++) sleep(200);
		ASSERT_EQ(ALOAD(&tfunv1), 0, "two v1");
		ASSERT_EQ(ALOAD(&tfunv2), 0, "two v2");
		ASSERT_EQ(ALOAD(&tfunv3), 1, "two v3");
		ASTORE(&state->value1, 1);
		exit(0);
	}

	waitid(P_PID, pid, NULL, WEXITED);

	munmap(state, sizeof(SharedStateData));
}

void sig_ign(i32);

Test(timeout4) {
	i32 i;

	/* Exercise sig_ign */
	sig_ign(0);

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

	_debug_set_timeout_fail = true;
	_debug_no_write = true;
	ASSERT(timeout(tfun1, U64_MAX), "set_timeout_fail");
	/* Exercise error code path for signals_init */
	signals_init();
	_debug_no_write = false;
	_debug_set_timeout_fail = false;
	ASSERT_BYTES(0);
}

Test(two1) {
	void *base = smap(sizeof(SharedStateData));
	i32 cpid;
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
	i32 cpid;
	SharedStateData *state = (SharedStateData *)base;
	state->uvalue1 = (u32)0;
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

u128 __umodti3(u128 a, u128 b);
u128 __udivti3(u128 a, u128 b);

Test(misc) {
	const u8 *s = "abcdefghi";
	u8 bufbig[200];
	u8 buf1[10];
	u8 buf2[10];
	u8 sx[100];
	i32 len;
	i128 v, v2;
	u128 y, x;

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
	memset((u8 *)buf2 + 3, 'b', 7);
	memorymove((u8 *)buf2 + 3, (u8 *)buf2, 7);
	ASSERT_EQ(buf2[3], 'a', "b0");

	ASSERT_EQ(u128_to_string(buf1, 100), 3, "100 len");
	ASSERT(!strcmpn(buf1, "100", 3), "100");
	ASSERT_EQ(u128_to_string(buf1, 0), 1, "0 len");
	ASSERT_EQ(buf1[0], '0', "0 value");
	ASSERT_EQ(i128_to_string(buf1, -10), 3, "-10 len");
	ASSERT_EQ(buf1[0], '-', "-10 value");
	ASSERT_EQ(buf1[1], '1', "-10 value2");
	ASSERT_EQ(buf1[2], '0', "-10 value3");
	ASSERT_EQ(i128_to_string(buf1, 10), 2, "10 len");

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
	len = u128_to_string(sx, U128_MAX);

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
	y = ((u128)1) << 64;
	x = __umodti3(y, ((u128)1) << 66);
	ASSERT_EQ(x, y, "big mod");

	ASSERT_EQ(double_to_string(bufbig, 0.0, 3), 1, "d2str0.0");
	ASSERT_EQ(bufbig[0], '0', "0");

	ASSERT(!strcmp(substrn("abcdefghi", "def", 9), "defghi"), "substrn");

	err = 0;
	ASSERT(
	    !string_to_uint128("999999999999999999999999999999999999999", 39),
	    "overflow");
	ASSERT_EQ(err, EINVAL, "einval");
	ASSERT_BYTES(0);
}

Test(udivti3) {
	u128 a, b, q;

	a = 16;
	b = ((u128)1) << 65;
	q = __udivti3(a, b);
	ASSERT_EQ(q, 0, "small_a_div");

	a = 100;
	b = 10;
	q = __udivti3(a, b);
	ASSERT_EQ(q, 10, "64bit_small_div");

	a = ((u128)1) << 80;
	b = 2;
	q = __udivti3(a, b);
	ASSERT_EQ(q, ((u128)1) << 79, "64bit_large_div");

	a = ((u128)100) << 65;
	b = ((u128)10) << 65;
	q = __udivti3(a, b);
	ASSERT_EQ(q, 10, "128bit_div");

	a = ((u128)1) << 65;
	b = ((u128)1) << 65;
	q = __udivti3(a, b);
	ASSERT_EQ(q, 1, "128bit_div");

	a = ((u128)1) << 100;
	b = ((u128)1) << 10;
	q = __udivti3(a, b);
	ASSERT_EQ(q, ((u128)1) << 90, "large_a_small_b_div");
}

Test(umodti3) {
	u128 a, b, r;

	a = 16;
	b = ((u128)1) << 65;
	r = __umodti3(a, b);
	ASSERT_EQ(r, 16, "small_a_mod");

	a = 100;
	b = 7;
	r = __umodti3(a, b);
	ASSERT_EQ(r, 2, "64bit_small_mod");

	a = ((u128)1) << 80;
	b = 3;
	r = __umodti3(a, b);
	ASSERT_EQ(r, 1, "64bit_large_mod");

	a = ((u128)100) << 65;
	b = ((u128)7) << 65;
	r = __umodti3(a, b);
	ASSERT_EQ(r, ((u128)2) << 65, "128bit_mod");

	a = ((u128)1) << 65;
	b = ((u128)1) << 65;
	r = __umodti3(a, b);
	ASSERT_EQ(r, 0, "equal_mod");

	a = ((u128)1) << 100;
	b = ((u128)1) << 10;
	r = __umodti3(a, b);
	ASSERT_EQ(r, 0, "large_a_small_b_mod");
}

Test(sysext) {
	const u8 *path = "/tmp/01234567789abc.txt";
	i32 fd;
	u128 v1, v2;

	getentropy(&v1, sizeof(u128));
	getentropy(&v2, sizeof(u128));
	ASSERT(v1 != v2, "getentropy");

	unlink(path);
	ASSERT(!exists(path), "!exists");
	fd = file(path);
	flush(fd);
	ASSERT(exists(path), "exists");
	close(fd);
	unlink(path);
}

Test(double_to_string) {
	u8 buf[64]; /* Ensure sufficient size */
	u64 len;

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
	ASSERT_BYTES(0);
}

Test(b64) {
	u8 buf[128];
	u8 buf2[128];
	u8 buf3[128];
	i32 len, len2;
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
static void assert_b64_eq(const u8 *out, const u8 *expected, const u8 *msg) {
	ASSERT(!strcmp((const u8 *)out, expected), msg);
}

Test(b642) {
	u8 buf[128];
	u8 buf2[128];
	u8 buf3[128];
	u64 len, len2;

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

Test(sys) {
	i32 fd, fd2;
	i32 pid = getpid();
	i32 ret = kill(pid, 0);
	i32 ret2 = kill(I32_MAX, 0);
	const u8 *path = "/tmp/systest.dat";
	ASSERT(!ret, "our pid");
	ASSERT(ret2, "invalid pid");
	err = 0;
	ASSERT(getrandom(NULL, 512, 0), "len>256");
	ASSERT_EQ(err, EIO, "eio");
	ASSERT(getrandom(NULL, 128, 0), "null buf");
	ASSERT_EQ(err, EFAULT, "efault");
	err = 0;
	settimeofday(NULL, NULL);
	ASSERT_EQ(err, EPERM, "eperm");

	unlink(path);
	fd = file(path);
	fd2 = fcntl(fd, F_DUPFD);
	ASSERT(fd != fd2, "ne");
	ASSERT(fd > 0, "fd>0");
	ASSERT(fd2 > 0, "fd2>0");
	close(fd);
	close(fd2);
	unlink(path);

	fd = epoll_create1(0);
	ASSERT(fd > 0, "epfd>0");
	ASSERT_EQ(epoll_ctl(fd, EPOLL_CTL_ADD, -1, NULL), -1, "epoll_ctl");
	ASSERT_EQ(err, EFAULT, "efault");
	err = 0;
	ASSERT_EQ(epoll_pwait(fd, NULL, 0, 1, NULL, 0), -1, "epoll_pwait");
	ASSERT_EQ(err, EINVAL, "einval");
	close(fd);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	ASSERT(fd > 0, "fd>0");
	ASSERT(shutdown(fd, 0), "shutdown");
	ASSERT(accept(fd, NULL, NULL), "accept");
	ASSERT(bind(fd, NULL, 0), "bind");
	ASSERT(connect(fd, NULL, 0), "connect");
	ASSERT(setsockopt(fd, 0, 0, NULL, 0), "setsockopt");
	ASSERT(getsockname(fd, NULL, NULL), "getsockname");
	ASSERT(!listen(fd, 0), "listen");
	ASSERT(getsockopt(0, 0, 0, NULL, NULL), "getsockopt is err");
	close(fd);
}

Test(large_multi_chunk) {
	Alloc *a;
	void *p;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	p = alloc_impl(a, CHUNK_SIZE * 10 - 16);
	ASSERT(p, "p != NULL");
	release_impl(a, p);
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(small_bitmap) {
	Alloc *a;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE / 2);
	ASSERT(!a, "a==NULL");
}

Test(concurrent_free) {
	Alloc *a;
	void *ptrs[10];
	i32 i;
	i32 fv;
	a = alloc_init(ALLOC_TYPE_SMAP, CHUNK_SIZE * 16);
	for (i = 0; i < 10; i++) {
		ptrs[i] = alloc_impl(a, CHUNK_SIZE / 4);
		ASSERT(ptrs[i], "alloc != NULL");
	}
	if ((fv = two())) {
		for (i = 0; i < 5; i++) {
			release_impl(a, ptrs[i]);
		}
	} else {
		for (i = 5; i < 10; i++) {
			release_impl(a, ptrs[i]);
		}
		exit(0);
	}
	waitid(P_PID, fv, NULL, WEXITED);
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(release_invalid_slab) {
	Alloc *a;
	void *p;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	p = alloc_impl(a, CHUNK_SIZE / 4);
	ASSERT(p, "p != NULL");
	_debug_no_exit = true;
	_debug_no_write = true;
	release_impl(a, (void *)((u64)p + 1));
	_debug_no_write = false;
	_debug_no_exit = false;
	release_impl(a, p);
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(multi_word_free) {
	Alloc *a;
	void *p;
	void *p2;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	p = alloc_impl(a, CHUNK_SIZE * 2);
	ASSERT(p, "p != NULL");
	release_impl(a, p);
	p2 = alloc_impl(a, CHUNK_SIZE * 2);
	ASSERT(p2, "p2 != NULL");
	release_impl(a, p2);
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(slab_capacity) {
	Alloc *a;
	void *p1;
	void *p2;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	p1 = alloc_impl(a, 8);
	p2 = alloc_impl(a, MAX_SLAB_SIZE);
	ASSERT(p1, "p1 != NULL");
	ASSERT(p2, "p2 != NULL");
	release_impl(a, p1);
	release_impl(a, p2);
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(alignment_check) {
	Alloc *a;
	void *p;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	p = alloc_impl(a, CHUNK_SIZE);
	ASSERT(p, "p != NULL");
	ASSERT((u64)p % 16 == 0, "aligned");
	release_impl(a, p);

	p = alloc_impl(a, 8);
	ASSERT(p, "p != NULL");
	ASSERT((u64)p % 8 == 0, "8aligned");
	release_impl(a, p);

	p = alloc_impl(a, 16);
	ASSERT(p, "p != NULL");
	ASSERT((u64)p % 16 == 0, "16aligned");
	release_impl(a, p);

	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(memsan_concurrent) {
	Alloc *a;
	void *ptrs[100];
	i32 i;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	for (i = 0; i < 48; i++) {
		ptrs[i] = alloc_impl(a, CHUNK_SIZE / 4);
		ASSERT(ptrs[i], "alloc != NULL");
	}
	for (i = 0; i < 48; i++) {
		release_impl(a, ptrs[i]);
	}
#if MEMSAN == 1
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
#endif
	alloc_destroy(a);
}

Test(resize_concurrent) {
	Alloc *a;
	void *ptrs[10];
	i32 i;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	for (i = 0; i < 10; i++) {
		ptrs[i] = alloc_impl(a, 8);
		ASSERT(ptrs[i], "alloc != NULL");
		ptrs[i] = resize_impl(a, ptrs[i], 16);
		ASSERT(ptrs[i], "resize != NULL");
	}
	for (i = 0; i < 10; i++) {
		release_impl(a, ptrs[i]);
	}
#if MEMSAN == 1
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
#endif
	alloc_destroy(a);
}

Test(slab_concurrent) {
	Alloc *a;
	void *ptrs[100];
	i32 i;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	for (i = 0; i < 100; i++) {
		ptrs[i] = alloc_impl(a, CHUNK_SIZE / 16);
		ASSERT(ptrs[i], "alloc != NULL");
	}
	for (i = 0; i < 100; i++) {
		release_impl(a, ptrs[i]);
	}
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
	ASSERT_BYTES(0);
}

Test(slab_failure) {
	Alloc *a;
	void *p1;
	void *p2;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE);
	p1 = alloc_impl(a, CHUNK_SIZE);
	ASSERT(p1, "p1 != NULL");
	err = 0;
	p2 = alloc_impl(a, CHUNK_SIZE / 4);
	ASSERT(!p2, "p2 == NULL");
	ASSERT_EQ(err, ENOMEM, "err == ENOMEM");
	release_impl(a, p1);
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(alloc_edge_cases) {
	Alloc *a;
	void *p1;
	void *p2;
	void *p3;
	void *p4;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	p1 = alloc_impl(a, MAX_SLAB_SIZE);
	p2 = alloc_impl(a, MAX_SLAB_SIZE + 1);
	p3 = alloc_impl(a, CHUNK_SIZE);
	p4 = alloc_impl(a, CHUNK_SIZE + 1);
	ASSERT(p1, "p1 != NULL");
	ASSERT(p2, "p2 != NULL");
	ASSERT(p3, "p3 != NULL");
	ASSERT(p4, "p4 != NULL");
	release_impl(a, p1);
	release_impl(a, p2);
	release_impl(a, p3);
	release_impl(a, p4);
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(multi_chunk_concurrent) {
	Alloc *a;
	void *ptrs[3];
	i32 i;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	for (i = 0; i < 3; i++) {
		ptrs[i] = alloc_impl(a, CHUNK_SIZE * 2);
		ASSERT(ptrs[i], "alloc != NULL");
	}
	for (i = 0; i < 3; i++) {
		release_impl(a, ptrs[i]);
	}
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
	ASSERT_BYTES(0);
}

Test(multi_chunk_fragmentation) {
	Alloc *a;
	void *ptrs[100];
	i32 i;
	void *p;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	for (i = 0; i < 12; i += 2) {
		ptrs[i] = alloc_impl(a, CHUNK_SIZE);
	}
	for (i = 0; i < 12; i += 2) {
		release_impl(a, ptrs[i]);
	}
	p = alloc_impl(a, CHUNK_SIZE * 3);
	ASSERT(p, "p != NULL");
	release_impl(a, p);
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(resize_edge_cases) {
	Alloc *a;
	void *p1;
	void *p2;
	void *p3;
	void *p4;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	p1 = alloc_impl(a, MAX_SLAB_SIZE);
	p2 = alloc_impl(a, MAX_SLAB_SIZE + 1);
	p3 = alloc_impl(a, CHUNK_SIZE);
	p4 = alloc_impl(a, CHUNK_SIZE + 1);
	ASSERT(p1, "p1 != NULL");
	ASSERT(p2, "p2 != NULL");
	ASSERT(p3, "p3 != NULL");
	ASSERT(p4, "p4 != NULL");
	p1 = resize_impl(a, p1, MAX_SLAB_SIZE + 1);
	p2 = resize_impl(a, p2, MAX_SLAB_SIZE);
	p3 = resize_impl(a, p3, CHUNK_SIZE + 1);
	p4 = resize_impl(a, p4, CHUNK_SIZE);
	ASSERT(p1, "p1 != NULL");
	ASSERT(p2, "p2 != NULL");
	ASSERT(p3, "p3 != NULL");
	ASSERT(p4, "p4 != NULL");
	release_impl(a, p1);
	release_impl(a, p2);
	release_impl(a, p3);
	release_impl(a, p4);
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
	ASSERT_BYTES(0);
}

Test(test_hex) {
	Formatter f = {0};
	format(&f, "x={x},y={X},z={},{c},{c}", 123456, 123456, 123456, 'x',
	       (u128)'y');
	ASSERT(
	    !strcmp(format_to_string(&f), "x=0x1e240,y=0x1E240,z=123456,x,y"),
	    "strcmp");
	format_clear(&f);
	format(&f, "x={X}", -0xccd);
	ASSERT(!strcmp(format_to_string(&f), "x=-0xCCD"), "strcmp2");
	format_clear(&f);
	ASSERT_BYTES(0);
}

void __stack_chk_fail(void);
void __stack_chk_guard(void);

Test(test_perror_close) {
	const char *path = "/tmp/perror_test";
	i32 fd;
	unlink(path);
	fd = file(path);
	ASSERT(fd > 0, "fd>0");
	ASSERT(!close(fd), "close");
	_debug_no_write = true;
	ASSERT(close(fd), "close fail");
	_debug_no_write = false;

	_debug_no_exit = true;
	_debug_no_write = true;
	__stack_chk_fail();
	__stack_chk_guard();
	_debug_no_exit = false;
	_debug_no_write = false;
	unlink(path);
}

Test(execve) {
	i32 pid, v;
	u8 *const argv[] = {"ls", "resources", NULL};
	if (!(pid = two2(false))) {
		close(2);
		close(1);
		execve("/xbin/ls", argv, NULL);
		exit(0);
	}
	ASSERT(!waitid(P_PID, pid, NULL, WEXITED), "waitid");
	if (!(pid = two2(false))) {
		close(2);
		close(1);
		execve("/bin/ls", argv, NULL);
		exit(0);
	}
	v = waitid(P_PID, pid, NULL, WEXITED);
	ASSERT_EQ(v, 0, "v");
}
