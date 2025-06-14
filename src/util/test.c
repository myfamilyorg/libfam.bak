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
#include <channel.H>
#include <error.H>
#include <lock.H>
#include <robust.H>
#include <syscall_const.H>
#include <test.H>

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
			LockGuard lg1;
			yield();
			lg1 = rlock(&state->lock1);
			if (state->value1) break;
		}
		ASSERT_EQ(state->value3++, 1, "val3");
		{
			LockGuard lg2 = rlock(&state->lock2);
			ASSERT_EQ(state->value2, 1, "val2");
			ASSERT_EQ(state->value3++, 3, "val3 final");
		}
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
	int cpid;
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	state->value3 = 0;

	if ((cpid = two())) {
		while (true) {
			LockGuard lg1;
			yield();
			lg1 = rlock(&state->lock1);
			if (state->value1) break;
		}
		ASSERT_EQ(state->value3++, 1, "val3 1");
		{
			LockGuard lg2 = wlock(&state->lock2);
			ASSERT_EQ(state->value2, 1, "val2 1");
			ASSERT_EQ(state->value3++, 3, "val3 3");
		}
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
	int cpid;
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	state->value3 = 0;

	if ((cpid = two())) {
		while (true) {
			sleepm(1);
			{
				LockGuard lg1 = rlock(&state->lock1);
				if (state->value1 == 2) break;
			}
		}
		{
			LockGuard lg2 = wlock(&state->lock2);
			ASSERT_EQ(state->value2, 2, "val2 2");
		}
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
	waitid(P_PID, cpid, NULL, WNOWAIT);
	munmap(state, sizeof(SharedStateData));
}

Test(lock4) {
	void *base = smap(sizeof(SharedStateData));
	int cpid;
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	state->value3 = 0;

	if ((cpid = two())) {
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
		if ((cpid = two())) {
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
			waitid(P_PID, cpid, NULL, WNOWAIT);
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

	waitid(P_PID, cpid, NULL, WNOWAIT);
	munmap(state, sizeof(SharedStateData));
}

Test(lock5) {
	void *base = smap(sizeof(SharedStateData));
	int size;
	int cpid;
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	size = 100;

	if ((cpid = two())) {
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
	waitid(P_PID, cpid, NULL, WNOWAIT);
	munmap(state, sizeof(SharedStateData));
}

typedef struct {
	RobustLock lock1;
	RobustLock lock2;
	int value1;
	int value2;
} RobustState;

Test(robust1) {
	RobustState *state = (RobustState *)smap(sizeof(RobustState));
	int cpid, i;
	state->lock1 = LOCK_INIT;
	state->value1 = 0;
	/* reap any zombie processes */
	for (i = 0; i < 10; i++) waitid(P_PID, 0, NULL, WNOWAIT);

	if ((cpid = two())) {
		waitid(P_PID, cpid, NULL, WNOWAIT);
	} else {
		if ((cpid = two())) {
			RobustGuard rg = robust_lock(&state->lock1);
			exit(0);
		} else {
			{
				sleepm(100);
				{
					RobustGuard rg =
					    robust_lock(&state->lock1);
					state->value1 = 1;
				}
			}
			exit(0);
		}
	}
	while (!ALOAD(&state->value1)) yield();
	munmap(state, sizeof(RobustState));
}

Test(robust2) {
	RobustState *state = (RobustState *)smap(sizeof(RobustState));
	int cpid, i;
	state->lock1 = LOCK_INIT;
	state->value1 = 0;
	/* reap any zombie processes */
	for (i = 0; i < 10; i++) waitid(P_PID, 0, NULL, WNOWAIT);

	if ((cpid = two())) {
		sleepm(10);
		{
			RobustGuard rg = robust_lock(&state->lock1);
			ASSERT_EQ(state->value1, 1, "value=1");
		}
		waitid(P_PID, cpid, NULL, WNOWAIT);
	} else {
		{
			RobustGuard rg = robust_lock(&state->lock1);
			sleepm(100);
			state->value1 = 1;
		}
		exit(0);
	}
	munmap(state, sizeof(RobustState));
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
		TestMessage msg = {0};
		msg.x = 1;
		msg.y = 2;
		send(&ch1, &msg);
		exit(0);
	}
}

Test(channel3) {
	int size = 1000, i;
	for (i = 0; i < size; i++) {
		int pid;
		Channel ch1 = channel(sizeof(TestMessage));
		err = 0;
		pid = two();
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
			TestMessage msg = {0};
			msg.x = 1;
			msg.y = 2;
			ASSERT(!send(&ch1, &msg), "send1");
			msg.x = 3;
			msg.y = 4;
			ASSERT(!send(&ch1, &msg), "send2");
			msg.x = 5;
			msg.y = 6;
			ASSERT(!send(&ch1, &msg), "send3");
			exit(0);
		}
		channel_destroy(&ch1);
		ASSERT_BYTES(0);
		waitid(P_PID, pid, NULL, WEXITED);
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
		sleepm(10);
		send(&ch2, &msg);

		recv(&ch1, &msg2);
		ASSERT_EQ(msg2.x, 1, "msg.x");
		ASSERT_EQ(msg2.y, 2, "msg.y");
		ASSERT_EQ(recv_now(&ch1, &msg), -1, "recv_now");

	} else {
		TestMessage msg = {0};
		recv(&ch2, &msg);

		ASSERT_EQ(msg.x, 100, "x=100");
		msg.x = 1;
		msg.y = 2;
		send(&ch1, &msg);
		exit(0);
	}
	waitid(P_PID, pid, &info, WEXITED);

	channel_destroy(&ch1);
	channel_destroy(&ch2);
}

Test(channel_cycle) {
	int pid, i;
	Channel ch1 = channel2(sizeof(TestMessage), 8);
	TestMessage msg;
	msg.x = 1;
	msg.y = 2;
	for (i = 0; i < 8; i++) send(&ch1, &msg);
	recv(&ch1, &msg);
	recv(&ch1, &msg);
	msg.x = 1;
	msg.y = 2;
	send(&ch1, &msg);

	if ((pid = two())) {
		msg.x = 0;
		recv(&ch1, &msg);
		ASSERT_EQ(msg.x, 1, "1");
	} else {
		sleepm(10);
		send(&ch1, &msg);
		exit(0);
	}
	waitid(P_PID, pid, NULL, WEXITED);
}

Test(channel_err) {
	TestMessage msg;
	int i;
	Channel ch1, ch2, ch3;

	ASSERT_BYTES(0);
	err = 0;
	ch1 = channel(0);
	ASSERT_EQ(err, EINVAL, "einval");
	ASSERT(!channel_ok(&ch1), "ok");
	err = 0;
	ch2 = channel2(8, 0);
	ASSERT_EQ(err, EINVAL, "einval2");
	ASSERT(!channel_ok(&ch1), "ok2");

	ch3 = channel2(sizeof(TestMessage), 8);
	for (i = 0; i < 8; i++) {
		msg.x = 1;
		msg.y = 2;
		ASSERT(!send(&ch3, &msg), "sendmsg");
	}
	msg.x = 1;
	msg.y = 2;
	ASSERT(send(&ch3, &msg), "senderr");

	channel_destroy(&ch1);
	channel_destroy(&ch2);
	channel_destroy(&ch3);

	ASSERT_BYTES(0);
}

