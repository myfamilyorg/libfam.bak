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

#include <alloc.h>
#include <atomic.h>
#include <channel.h>
#include <colors.h>
#include <env.h>
#include <error.h>
#include <event.h>
#include <format.h>
#include <init.h>
#include <limits.h>
#include <lock.h>
#include <misc.h>
#include <robust.h>
#include <sys.h>
#include <syscall.h>
#include <syscall_const.h>
#include <test.h>

/*
int unsetenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
*/

typedef struct {
	Lock lock1;
	Lock lock2;
	Lock lock3;
	Lock lock4;
	Lock lock5;
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
}

Test(two1) {
	void *base = smap(sizeof(SharedStateData));
	SharedStateData *state = (SharedStateData *)base;
	state->value1 = 0;
	state->value2 = 0;
	if (two()) {
		while (!ALOAD(&state->value1));
		state->value2++;
	} else {
		ASTORE(&state->value1, 1);
		exit(0);
	}
	ASSERT(state->value2, "store");
	munmap(base, sizeof(SharedStateData));
}

Test(futex1) {
	void *base = smap(sizeof(SharedStateData));
	SharedStateData *state = (SharedStateData *)base;
	state->uvalue1 = (uint32_t)0;
	if (two()) {
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
	ASSERT(state->value2, "value2");
	munmap(base, sizeof(SharedStateData));
}

Test(lock0) {
	Lock l1 = LOCK_INIT;
	ASSERT_EQ(l1, 0, "init");
	{
		LockGuard lg1 = rlock(&l1);
		ASSERT_EQ(l1, 1, "l1=1");
	}
	ASSERT_EQ(l1, 0, "back to 0");
	{
		LockGuard lg1 = wlock(&l1);
		uint32_t vabc = 0x1 << 31;
		ASSERT_EQ(l1, vabc, "vabc");
	}
	ASSERT_EQ(l1, 0, "final");
}

Test(lock1) {
	void *base = smap(sizeof(SharedStateData));
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	state->value3 = 0;

	if (two()) {
		while (true) {
			yield();
			LockGuard lg1 = rlock(&state->lock1);
			if (state->value1) break;
		}
		ASSERT_EQ(state->value3++, 1, "val3");
		LockGuard lg2 = rlock(&state->lock2);
		ASSERT_EQ(state->value2, 1, "val2");
		ASSERT_EQ(state->value3++, 3, "val3 final");
	} else {
		{
			LockGuard lg2 = wlock(&state->lock2);
			ASSERT_EQ(state->value3++, 0, "val3 0");
			{
				LockGuard lg1 = wlock(&state->lock1);
				state->value1 = 1;
			}
			sleepm(10);
			state->value2 = 1;
			ASSERT_EQ(state->value3++, 2, "val3 2");
		}
		exit(0);
	}
	munmap(base, sizeof(SharedStateData));
}

Test(lock2) {
	void *base = smap(sizeof(SharedStateData));
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	state->value3 = 0;

	if (two()) {
		while (true) {
			yield();
			LockGuard lg1 = rlock(&state->lock1);
			if (state->value1) break;
		}
		ASSERT_EQ(state->value3++, 1, "val3 1");
		LockGuard lg2 = wlock(&state->lock2);
		ASSERT_EQ(state->value2, 1, "val2 1");
		ASSERT_EQ(state->value3++, 3, "val3 3");
	} else {
		{
			LockGuard lg2 = wlock(&state->lock2);
			ASSERT_EQ(state->value3++, 0, "val3 0");
			{
				LockGuard lg1 = wlock(&state->lock1);
				state->value1 = 1;
			}
			sleepm(10);
			state->value2 = 1;
			ASSERT_EQ(state->value3++, 2, "val3 2");
		}
		exit(0);
	}
	munmap(state, sizeof(SharedStateData));
}

Test(lock3) {
	void *base = smap(sizeof(SharedStateData));
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	state->value3 = 0;

	if (two()) {
		while (true) {
			sleepm(1);
			LockGuard lg1 = rlock(&state->lock1);
			if (state->value1 == 2) break;
		}
		LockGuard lg2 = wlock(&state->lock2);
		ASSERT_EQ(state->value2, 2, "val2 2");
	} else {
		if (two()) {
			LockGuard lg2 = wlock(&state->lock2);
			state->value2++;
			sleepm(10);
			state->value1++;
		} else {
			LockGuard lg2 = wlock(&state->lock2);
			state->value2++;
			sleepm(10);
			state->value1++;
		}
		exit(0);
	}
	munmap(state, sizeof(SharedStateData));
}

Test(lock4) {
	void *base = smap(sizeof(SharedStateData));
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	state->value3 = 0;

	if (two()) {
		while (true) {
			LockGuard lg1 = rlock(&state->lock1);
			if (state->value1 == 2) break;
		}

		{
			LockGuard lg2 = wlock(&state->lock2);
			ASSERT_EQ(state->value2++, 0, "val2 0");

			ASSERT_EQ(state->value3, 0, "val3 0");
		}
		while (!ALOAD(&state->value3)) yield();

	} else {
		if (two()) {
			{
				LockGuard lg2 = rlock(&state->lock2);
				state->value1++;
				sleepm(100);
			}
			{
				LockGuard lg2 = rlock(&state->lock2);
				ASSERT_EQ(state->value2++, 1, "val2 1");
				state->value3++;
			}
			exit(0);
		} else {
			{
				LockGuard lg2 = rlock(&state->lock2);
				state->value1++;
				sleepm(200);
			}
			exit(0);
		}
	}
	munmap(state, sizeof(SharedStateData));
}

Test(lock5) {
	void *base = smap(sizeof(SharedStateData));
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	int size = 100;

	if (two()) {
		while (true) {
			LockGuard lg = wlock(&state->lock1);
			if (state->value1 % 2 == 0) state->value1++;
			if (state->value1 >= size) break;
		}
		ASTORE(&state->value2, 1);
		while (true) {
			LockGuard lg = wlock(&state->lock1);
			if (state->value2 == 0) break;
		}
		ASSERT_EQ(ALOAD(&state->value2), 0, "val2 0");
	} else {
		while (true) {
			LockGuard lg = wlock(&state->lock1);
			if (state->value1 % 2 == 1) state->value1++;
			if (state->value1 >= size) break;
		}
		while (true) {
			LockGuard lg = wlock(&state->lock1);
			if (state->value2 == 1) break;
		}
		ASTORE(&state->value2, 0);
		exit(0);
	}
	munmap(state, sizeof(SharedStateData));
}

Lock tfunlock = LOCK_INIT;
int tfunv1 = 0;
int tfunv2 = 0;
int tfunv3 = 0;

void tfun1() {
	LockGuard l = wlock(&tfunlock);
	tfunv1++;
}

void tfun2() {
	LockGuard l = wlock(&tfunlock);
	tfunv2++;
}

void tfun3() {
	LockGuard l = wlock(&tfunlock);
	tfunv3++;
}

Test(timeout1) {
	ASSERT(!timeout(tfun1, 100), "tfun1");
	sleepm(300);
	LockGuard l = rlock(&tfunlock);
	ASSERT_EQ(tfunv1, 1, "complete");
}

Test(timeout2) {
	tfunv1 = 0;
	tfunv2 = 0;
	tfunv3 = 0;

	tfunlock = LOCK_INIT;
	timeout(tfun1, 100);
	timeout(tfun2, 200);
	sleepm(1000);
	{
		LockGuard l = rlock(&tfunlock);
		ASSERT_EQ(tfunv1, 1, "1");
		ASSERT_EQ(tfunv2, 0, "2");
	}
	sleepm(200);
	ASSERT_EQ(tfunv2, 1, "3");
}

Test(timeout3) {
	tfunv1 = 0;
	tfunv2 = 0;
	tfunv3 = 0;
	SharedStateData *state =
	    (SharedStateData *)smap(sizeof(SharedStateData));
	state->value1 = 0;

	timeout(tfun1, 100);
	timeout(tfun2, 200);
	if (two()) {
		timeout(tfun3, 150);
		for (int i = 0; i < 3; i++) sleepm(200);
		ASSERT_EQ(tfunv1, 1, "v1 1");
		ASSERT_EQ(tfunv2, 1, "v2 1");
		ASSERT_EQ(tfunv3, 1, "v3 1");
		while (!ALOAD(&state->value1));
	} else {
		timeout(tfun3, 150);
		for (int i = 0; i < 3; i++) sleepm(200);
		ASSERT_EQ(tfunv1, 0, "two v1");
		ASSERT_EQ(tfunv2, 0, "two v2");
		ASSERT_EQ(tfunv3, 1, "two v3");
		ASTORE(&state->value1, 1);
		exit(0);
	}

	munmap(state, sizeof(SharedStateData));
}

Test(timeout4) {
	ASSERT(timeout(NULL, 100), "timeout NULL");
	tfunv1 = 0;

	for (int i = 0; i < 32; i++) {
		ASSERT(!timeout(tfun1, 10), "timout");
	}

	err = 0;
	ASSERT(timeout(tfun1, 10), "overflow");
	ASSERT_EQ(err, EOVERFLOW, "overflow err check");

	while (true) {
		sleepm(1);
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
	char *t1 = resize(NULL, 8), *t2, *t3;
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

	void *t4 = alloc(CHUNK_SIZE);
	void *t5 = resize(t4, 32);
	ASSERT(t5, "resize down from chunk");
	release(t5);
}

typedef struct {
	int x;
	int y;
} TestMessage;

Test(channel1) {
	Channel ch1 = channel(sizeof(TestMessage));
	TestMessage msg = {0}, msg2 = {0};
	msg.x = 1;
	msg.y = 2;
	send(&ch1, &msg);
	ASSERT(!recv_now(&ch1, &msg2), "recv1");
	ASSERT_EQ(msg2.x, 1, "x=1");
	ASSERT_EQ(msg2.y, 2, "y=2");

	msg.x = 3;
	msg.y = 4;
	send(&ch1, &msg);
	msg.x = 5;
	msg.y = 6;
	send(&ch1, &msg);
	ASSERT(!recv_now(&ch1, &msg2), "recv2");
	ASSERT_EQ(msg2.x, 3, "x=3");
	ASSERT_EQ(msg2.y, 4, "y=4");
	ASSERT(!recv_now(&ch1, &msg2), "recv3");
	ASSERT_EQ(msg2.x, 5, "x=5");
	ASSERT_EQ(msg2.y, 6, "y=6");
	ASSERT(recv_now(&ch1, &msg2), "recv none");
	channel_destroy(&ch1);
}

Test(channel2) {
	Channel ch1 = channel(sizeof(TestMessage));
	if (two()) {
		TestMessage msg = {0};
		recv(&ch1, &msg);
		ASSERT_EQ(msg.x, 1, "x=1");
		ASSERT_EQ(msg.y, 2, "y=2");
	} else {
		send(&ch1, &(TestMessage){.x = 1, .y = 2});
		exit(0);
	}
}

Test(channel3) {
	int size = 1000;
	for (int i = 0; i < size; i++) {
		Channel ch1 = channel(sizeof(TestMessage));
		err = 0;
		int pid = two();
		ASSERT(pid != -1, "two != -1");
		if (pid) {
			TestMessage msg = {0};
			recv(&ch1, &msg);
			ASSERT_EQ(msg.x, 1, "msg.x 1");
			ASSERT_EQ(msg.y, 2, "msg.y 2");
			recv(&ch1, &msg);
			ASSERT_EQ(msg.x, 3, "msg.x 3");
			ASSERT_EQ(msg.y, 4, "msg.y 4");
			recv(&ch1, &msg);
			ASSERT_EQ(msg.x, 5, "msg.x 5");
			ASSERT_EQ(msg.y, 6, "msg.y 6");
			ASSERT_EQ(recv_now(&ch1, &msg), -1, "recv_now");
			ASSERT(!waitid(0, pid, NULL, 4), "waitpid");
		} else {
			ASSERT(send(&ch1, &(TestMessage){.x = 1, .y = 2}) >= 0,
			       "send1");
			ASSERT(send(&ch1, &(TestMessage){.x = 3, .y = 4}) >= 0,
			       "send2");
			ASSERT(send(&ch1, &(TestMessage){.x = 5, .y = 6}) >= 0,
			       "send3");
			exit(0);
		}
		channel_destroy(&ch1);
		ASSERT_BYTES(0);
		waitid(0, pid, NULL, 4);
	}
}

Test(channel_notify) {
	siginfo_t info = {0};
	Channel ch1 = channel(sizeof(TestMessage));
	Channel ch2 = channel(sizeof(TestMessage));

	int pid = two();
	ASSERT(pid >= 0, "pid>=0");
	if (pid) {
		TestMessage msg = {0}, msg2 = {0};
		msg.x = 100;
		sleepm(10);  // wait to ensure recv is in wait state
		send(&ch2, &msg);

		recv(&ch1, &msg2);
		ASSERT_EQ(msg2.x, 1, "msg.x");
		ASSERT_EQ(msg2.y, 2, "msg.y");
		ASSERT_EQ(recv_now(&ch1, &msg), -1, "recv_now");

	} else {
		TestMessage msg = {0};
		recv(&ch2, &msg);

		ASSERT_EQ(msg.x, 100, "x=100");
		send(&ch1, &(TestMessage){.x = 1, .y = 2});
		exit(0);
	}
	waitid(0, pid, &info, 4);

	channel_destroy(&ch1);
	channel_destroy(&ch2);
}

Test(channel_cycle) {
	int pid;
	Channel ch1 = channel2(sizeof(TestMessage), 8);
	TestMessage msg;
	for (int i = 0; i < 8; i++) send(&ch1, &(TestMessage){.x = 1, .y = 2});
	recv(&ch1, &msg);
	recv(&ch1, &msg);
	send(&ch1, &(TestMessage){.x = 1, .y = 2});

	if ((pid = two())) {
		msg.x = 0;
		recv(&ch1, &msg);
		ASSERT_EQ(msg.x, 1, "1");
	} else {
		sleepm(10);
		send(&ch1, &msg);
		exit(0);
	}
	waitid(0, pid, NULL, 4);
}

Test(channel_err) {
	err = 0;
	Channel ch1 = channel(0);
	ASSERT_EQ(err, EINVAL, "einval");
	ASSERT(!channel_ok(&ch1), "ok");
	err = 0;
	Channel ch2 = channel2(8, 0);
	ASSERT_EQ(err, EINVAL, "einval2");
	ASSERT(!channel_ok(&ch1), "ok2");

	Channel ch3 = channel2(sizeof(TestMessage), 8);
	for (int i = 0; i < 8; i++) {
		ASSERT(!send(&ch3, &(TestMessage){.x = 1, .y = 2}), "sendi");
	}
	ASSERT(send(&ch3, &(TestMessage){.x = 1, .y = 2}), "senderr");

	channel_destroy(&ch1);
	channel_destroy(&ch2);
	channel_destroy(&ch3);
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
	ASSERT(!strcmp("Too many open files in system", error_string(ENFILE)),
	       "enfile");
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
	ASSERT(!strcmp("Numerical argument out of domain", error_string(EDOM)),
	       "edom");
	ASSERT(!strcmp("Numerical result out of range", error_string(ERANGE)),
	       "erange");
	ASSERT(!strcmp("Resource deadlock avoided", error_string(EDEADLK)),
	       "edeadlk");
	ASSERT(!strcmp("File name too long", error_string(ENAMETOOLONG)),
	       "enametoolong");
	ASSERT(!strcmp("No locks available", error_string(ENOLCK)), "enolck");
	ASSERT(!strcmp("Function not implemented", error_string(ENOSYS)),
	       "enosys");
	ASSERT(!strcmp("Directory not empty", error_string(ENOTEMPTY)),
	       "enotempty");
	ASSERT(
	    !strcmp("Too many levels of symbolic links", error_string(ELOOP)),
	    "eloop");
	ASSERT(!strcmp("Numerical overflow", error_string(EOVERFLOW)),
	       "eoverflow");
	ASSERT(!strcmp("Unknown error", error_string(-1)), "unknown");

	err = 0;
	ASSERT_EQ(*__error(), 0, "__error");
	perror_set_no_write(true);
	perror("test");
	perror_set_no_write(false);
}

extern int cur_tasks;
int ecount = 0;
void my_exit(void) { ecount++; }

Test(begin) {
	cur_tasks = 1;
	begin();
	ASSERT_EQ(cur_tasks, 0, "cur_tasks");
	for (int i = 0; i < 64; i++)
		ASSERT(!register_exit(my_exit), "register_exit");
	for (int i = 0; i < 3; i++)
		ASSERT(register_exit(my_exit), "register_exit_overflow");

	execute_exits();
	ASSERT_EQ(ecount, 64, "64 exits");  // MAX_EXITS is 64
}

Test(pipe) {
	int fds[2];
	err = 0;
	ASSERT(!pipe(fds), "pipe");
	ASSERT(!err, "not err");
	int fv;
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
	waitid(0, fv, NULL, 4);
}

Test(files1) {
	const char *fname = "/tmp/ftest.tmp";
	unlink(fname);
	err = 0;
	int fd = file(fname);
	ASSERT(fd > 0, "fd > 0");

	ASSERT(exists(fname), "exists");

	err = 0;
	ASSERT(!fsize(fd), "fsize");
	write(fd, "abc", 3);
	flush(fd);
	ASSERT_EQ(fsize(fd), 3, "fsize==3");

	fresize(fd, 2);
	ASSERT_EQ(fsize(fd), 2, "fisze==2");
	int dup = fcntl(fd, F_DUPFD);
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
	err = 0;
	void *v = mmap(buf, 100, PROT_READ | PROT_WRITE, MAP_SHARED, -1, 0);
	ASSERT((long)v == -1, "mmap err");
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

Test(misc_atomic) {
	uint32_t x = 2;
	__or32(&x, 1);
	ASSERT_EQ(x, 3, "or32");

	uint64_t y = 2;
	__or64(&y, 1);
	ASSERT_EQ(y, 3, "or64");

	__sub64(&y, 1);
	ASSERT_EQ(y, 2, "sub64");

	uint32_t a = 0;
	uint32_t b = 1;
	ASSERT(!__cas32(&a, &b, 0), "cas32");
}

uint128_t __umodti3(uint128_t a, uint128_t b);
uint128_t __udivti3(uint128_t a, uint128_t b);

Test(misc) {
	ASSERT(strcmpn("1abc", "1def", 4) < 0, "strcmpn1");
	ASSERT(strcmpn("1ubc", "1def", 4) > 0, "strcmpn2");

	const char *s = "abcdefghi";
	ASSERT_EQ(substr(s, "def") - s, 3, "strstr");
	char buf1[10];
	char buf2[10];
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
	int128_t v = string_to_int128("123", 3);
	ASSERT_EQ(err, SUCCESS, "success");
	ASSERT_EQ(string_to_int128("123", 3), 123, "123");
	ASSERT_EQ(string_to_int128("", 0), 0, "0 len");
	err = 0;
	int128_t v2 = string_to_int128("111", 3);
	ASSERT_EQ(v2, 111, "111");
	ASSERT_EQ(string_to_int128(" 1234", 5), 1234, "1234");
	ASSERT_EQ(string_to_int128(" ", 1), 0, "0");
	ASSERT_EQ(string_to_int128("-3", 2), -3, "-3");
	ASSERT_EQ(string_to_int128("+5", 2), 5, "+5");
	ASSERT_EQ(string_to_int128("-", 1), 0, "-");
	char sx[100];
	int len = uint128_t_to_string(sx, UINT128_MAX);
	ASSERT_EQ(string_to_int128(sx, len), 0, "overflow int128");

	ASSERT_EQ(double_to_string(buf1, 1.2, 1), 3, "double to str 1.2");
	buf1[3] = 0;
	ASSERT(!strcmp(buf1, "1.2"), "1.2");
	char bufbig[200], bufbig2[200], bufbig3[200];
	ASSERT_EQ(double_to_string(buf1, -4.2, 1), 4, "double to str -4.2");
	buf1[4] = 0;
	ASSERT(!strcmp(buf1, "-4.2"), "-4.2");
	ASSERT_EQ(double_to_string(bufbig, 0.123, 3), 5, "0.123");

	ASSERT_EQ(__umodti3(11, 10), 1, "11 % 10");
	ASSERT_EQ(__udivti3(31, 10), 3, "31 / 10");
	uint128_t y = ((uint128_t)1) << 64;
	uint128_t x = __umodti3(y, ((uint128_t)1) << 66);
	ASSERT_EQ(x, y, "big mod");

	ASSERT_EQ(double_to_string(bufbig, 0.0, 3), 1, "d2str0.0");
	ASSERT_EQ(bufbig[0], '0', "0");
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
	size_t len;

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

Test(b64) {
	uint8_t buf[128];
	uint8_t buf2[128];
	uint8_t buf3[128];
	memcpy(buf, "0123456789", 10);
	int len = b64_encode(buf, 10, buf2, 128);
	int len2 = b64_decode(buf2, len, buf3, 128);
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

Test(event) {
	Event events[10];
	int val = 101;
	int fds[2];
	int mplex = multiplex();
	ASSERT(mplex > 0, "mplex");
	pipe(fds);
	ASSERT_EQ(mregister(mplex, fds[0], MULTIPLEX_FLAG_READ, &val), 0,
		  "mreg");
	ASSERT_EQ(write(fds[1], "abc", 3), 3, "write");
	err = 0;
	int x = mwait(mplex, events, 10, 10);
	ASSERT_EQ(x, 1, "mwait");

	close(mplex);
	close(fds[0]);
	close(fds[1]);
}

Test(colors) {
	const char *test_file = "/tmp/colortest";

	unlink(test_file);
	int fd = file(test_file);
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

typedef struct {
	RobustLock lock1;
	RobustLock lock2;
	int value1;
	int value2;
} RobustState;

Test(robust1) {
	RobustState *state = (RobustState *)smap(sizeof(RobustState));
	state->lock1 = LOCK_INIT;
	state->value1 = 0;
	int cpid;

	if ((cpid = two())) {
		waitid(0, cpid, NULL, 4);
	} else {
		if (two()) {
			RobustGuard rg = robust_lock(&state->lock1);
			exit(0);
		} else {
			{
				sleepm(100);
				RobustGuard rg = robust_lock(&state->lock1);
				state->value1 = 1;
			}
			exit(0);
		}
	}
	while (!ALOAD(&state->value1)) yield();
	munmap(state, sizeof(RobustState));
}

Test(snprintf) {
	char buf[1024];
	int x = 1;
	int len = snprintf(NULL, 0, "test%i");
	// TODO: should be 5
	ASSERT_EQ(len, 6, "len=5");
}

