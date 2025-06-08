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
#include <env.h>
#include <error.h>
#include <init.h>
#include <limits.h>
#include <lock.h>
#include <stdio.h>
#include <sys.h>
#include <syscall.h>
#include <syscall_const.h>
#include <test.h>

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
			yield();
			LockGuard lg1 = rlock(&state->lock1);
			if (state->value1 == 2) break;
		}
		LockGuard lg2 = wlock(&state->lock2);
		ASSERT_EQ(state->value2, 2, "val2 2");
	} else {
		if (two()) {
			LockGuard lg2 = rlock(&state->lock2);
			state->value1++;
			sleepm(10);
			state->value2++;
		} else {
			LockGuard lg2 = rlock(&state->lock2);
			state->value1++;
			sleepm(10);
			state->value2++;
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
	ASSERT_BYTES(0);
}

typedef struct {
	int x;
	int y;
} TestMessage;

Test(channel1) {
	Channel ch1 = channel(sizeof(TestMessage));
	if (two()) {
		TestMessage msg = {0};
		while (true) {
			int res = recv_now(&ch1, &msg);
			if (res == -1) continue;
			ASSERT_EQ(msg.x, 1, "msg.x");
			ASSERT_EQ(msg.y, 2, "msg.y");
			break;
		}
		ASSERT_EQ(recv_now(&ch1, &msg), -1, "recv_now");
	} else {
		send(&ch1, &(TestMessage){.x = 1, .y = 2});
		exit(0);
	}
	channel_destroy(&ch1);
}

Test(channel2) {
	Channel ch1 = channel(sizeof(TestMessage));
	if (two()) {
		TestMessage msg = {0};
		recv(&ch1, &msg);
		ASSERT_EQ(msg.x, 1, "msg.x");
		ASSERT_EQ(msg.y, 2, "msg.y");
		ASSERT_EQ(recv_now(&ch1, &msg), -1, "recv_now");
	} else {
		send(&ch1, &(TestMessage){.x = 1, .y = 2});
		exit(0);
	}
	channel_destroy(&ch1);
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
			err = 0;
			waitid(P_PID, pid, NULL, 0);

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
	}
}

int *__error(void);

Test(errors) {
	ASSERT(!strcmp("Success", error_string(SUCCESS)), "success");
	ASSERT(!strcmp("Operation not permitted", error_string(EPERM)),
	       "eperm");
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
}

