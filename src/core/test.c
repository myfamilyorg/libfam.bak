#include <criterion/criterion.h>
#include <lock.h>
#include <robust.h>
#include <sys.h>
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
} SharedStateData;

Test(core, lock1) {
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
			if (state->value1) break;
		}
		cr_assert_eq(state->value3++, 1);
		LockGuard lg2 = rlock(&state->lock2);
		cr_assert_eq(state->value2, 1);
		cr_assert_eq(state->value3++, 3);
	} else {
		{
			LockGuard lg2 = wlock(&state->lock2);
			cr_assert_eq(state->value3++, 0);
			{
				LockGuard lg1 = wlock(&state->lock1);
				state->value1 = 1;
			}
			sleepm(10);
			state->value2 = 1;
			cr_assert_eq(state->value3++, 2);
		}
		exit(0);
	}
}

Test(core, lock2) {
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
			if (state->value1) break;
		}
		cr_assert_eq(state->value3++, 1);
		LockGuard lg2 = wlock(&state->lock2);
		cr_assert_eq(state->value2, 1);
		cr_assert_eq(state->value3++, 3);
	} else {
		{
			LockGuard lg2 = wlock(&state->lock2);
			cr_assert_eq(state->value3++, 0);
			{
				LockGuard lg1 = wlock(&state->lock1);
				state->value1 = 1;
			}
			sleepm(10);
			state->value2 = 1;
			cr_assert_eq(state->value3++, 2);
		}
		exit(0);
	}
}

Test(core, lock3) {
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
		LockGuard lg2 = wlock(&state->lock2);
		cr_assert_eq(state->value2, 2);
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
}

Test(core, lock4) {
	void *base = smap(sizeof(SharedStateData));
	SharedStateData *state = (SharedStateData *)base;
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	state->value2 = 0;
	state->value3 = 0;

	if (two()) {
		// Wait for both children to acquire lock2
		while (true) {
			LockGuard lg1 = rlock(&state->lock1);
			if (state->value1 == 2) break;
		}

		{
			// parent first to request write lock because children
			// sleep 100 and 200ms respectively. But doesn't acquire
			// lock until both release their read locks
			// this shows write starvation prevention
			LockGuard lg2 = wlock(&state->lock2);
			cr_assert_eq(state->value2++,
				     0);  // First to set value2 to 1

			cr_assert_eq(state->value3, 0);
		}

		// ensure that child1 succeeds and is second to update value2
		while (!ALOAD(&state->value3));

	} else {
		if (two()) {
			{
				LockGuard lg2 = rlock(&state->lock2);
				state->value1++;
				sleepm(100);
			}
			{
				// child1 will request read lock after writer,
				// but get read after write has released (write
				// prioritized once requested)
				LockGuard lg2 = rlock(&state->lock2);
				cr_assert_eq(state->value2++, 1);
				state->value3++;
			}
			exit(0);
		} else {
			{
				// child2 hold lock to ensure ordering of
				// writer/reader
				LockGuard lg2 = rlock(&state->lock2);
				state->value1++;
				sleepm(200);
			}
			exit(0);
		}
	}
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

Test(core, timeout1) {
	timeout(tfun1, 100);
	sleepm(1000);
	LockGuard l = rlock(&tfunlock);
	cr_assert_eq(tfunv1, 1);
}

Test(core, timeout2) {
	timeout(tfun1, 100);
	timeout(tfun2, 200);
	sleepm(1000);  // sleepm interrupted at first SIGALRM (100 ms, second
		       // alarm hasn't occurred yet).
	{
		LockGuard l = rlock(&tfunlock);
		cr_assert_eq(tfunv1, 1);
		cr_assert_eq(tfunv2, 0);
	}
	sleepm(200);  // by now second alarm went off
	cr_assert_eq(tfunv2, 1);
}

Test(core, timeout3) {
	SharedStateData *state =
	    (SharedStateData *)smap(sizeof(SharedStateData));
	state->value1 = 0;

	timeout(tfun1, 100);
	timeout(tfun2, 200);
	if (two()) {
		timeout(tfun3, 150);
		for (int i = 0; i < 3; i++) sleepm(200);
		cr_assert_eq(tfunv1, 1);
		cr_assert_eq(tfunv2, 1);
		cr_assert_eq(tfunv3, 1);
		while (!ALOAD(&state->value1));
	} else {
		timeout(tfun3, 150);
		for (int i = 0; i < 3; i++) sleepm(200);
		cr_assert_eq(tfunv1, 0);
		cr_assert_eq(tfunv2, 0);
		cr_assert_eq(tfunv3, 1);
		ASTORE(&state->value1, 1);
		exit(0);
	}
}

Test(core, robust1) {
	RobustLock l1 = ROBUST_LOCK_INIT;
	cr_assert(!l1);
	RobustGuard rg = robust_lock(&l1);
	cr_assert(l1);
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

	if (two()) {
		while (true) {
			RobustGuard rg = robust_lock(&state->lock);
			if (ALOAD(&state->value) == 1) break;
		}
	} else {
		// ensure we don't unlock by using RobustGuardImpl
		// (without RAII)
		RobustGuardImpl rg = robust_lock(&state->lock);
		sleepm(300);
		AADD(&state->value, 1);
		exit(0);
	}
}

Test(core, robust3) {
	void *base = smap(sizeof(RobustSharedState));
	RobustSharedState *state = (RobustSharedState *)base;
	state->lock = ROBUST_LOCK_INIT;
	state->value = 0;

	if (two()) {
		while (true) {
			RobustGuard rg = robust_lock(&state->lock);
			if (ALOAD(&state->value) == 1) break;
		}
		cr_assert_eq(ALOAD(&state->value), 1);
	} else {
		RobustGuardImpl rg = robust_lock(&state->lock);
		AADD(&state->value, 1);
		exit(0);
	}
}

Test(core, robust4) {
	void *base = smap(sizeof(RobustSharedState));
	RobustSharedState *state = (RobustSharedState *)base;
	state->lock = ROBUST_LOCK_INIT;
	state->value = 0;
	state->value2 = 0;

	if (two()) {
		while (true) {
			RobustGuard rg = robust_lock(&state->lock);
			if (ALOAD(&state->value) % 2 == 0)
				AADD(&state->value, 1);
			if (ALOAD(&state->value) >= 11) break;
		}
		ASTORE(&state->value2, 1);
		while (true) {
			RobustGuard rg = robust_lock(&state->lock);
			if (ALOAD(&state->value2) == 0) break;
		}
		cr_assert_eq(ALOAD(&state->value2), 0);
	} else {
		while (true) {
			RobustGuard rg = robust_lock(&state->lock);
			if (ALOAD(&state->value) % 2 == 1) {
				AADD(&state->value, 1);
			}
			if (ALOAD(&state->value) >= 11) break;
		}
		while (true) {
			RobustGuard rg = robust_lock(&state->lock);
			if (ALOAD(&state->value2) == 1) break;
		}
		ASTORE(&state->value2, 0);
		exit(0);
	}
}

Test(core, robust_non_contended) {
	void *base = smap(sizeof(RobustSharedState));
	RobustSharedState *state = (RobustSharedState *)base;
	state->lock = ROBUST_LOCK_INIT;
	state->value = 0;

	for (int i = 0; i < 1000; i++) {
		RobustGuard rg = robust_lock(&state->lock);
		AADD(&state->value, 1);
	}
	cr_assert_eq(ALOAD(&state->value), 1000);
}

Test(core, robust_multi_process) {
	void *base = smap(sizeof(RobustSharedState));
	RobustSharedState *state = (RobustSharedState *)base;
	state->lock = ROBUST_LOCK_INIT;
	state->value = 0;
	const int N = 10;
	pid_t pids[N];

	for (int i = 0; i < N; i++) {
		if ((pids[i] = two()) == 0) {
			while (ALOAD(&state->value) < 1000) {
				RobustGuard rg = robust_lock(&state->lock);
				if (ALOAD(&state->value) < 1000) {
					AADD(&state->value, 1);
				}
			}
			exit(0);
		}
	}
	struct timespec start, now;
	clock_gettime(CLOCK_MONOTONIC, &start);
	while (ALOAD(&state->value) < 1000) {
		RobustGuard rg = robust_lock(&state->lock);
		clock_gettime(CLOCK_MONOTONIC, &now);
		if ((now.tv_sec - start.tv_sec) > 5) {	// 5s timeout
			cr_assert(0, "Timeout waiting for value >= 1000");
		}
	}
	cr_assert_eq(ALOAD(&state->value), 1000);
	for (int i = 0; i < N; i++) {
		waitpid(pids[i], NULL, 0);
	}
}

Test(core, robust_port_exhaustion) {
	void *base = smap(sizeof(RobustSharedState));
	RobustSharedState *state = (RobustSharedState *)base;
	state->lock = ROBUST_LOCK_INIT;
	state->value = 0;
	const int N = 1000;

	for (int i = 0; i < N; i++) {
		RobustGuard rg = robust_lock(&state->lock);
		AADD(&state->value, 1);
	}
	cr_assert_eq(ALOAD(&state->value), N);
}

Test(core, robust_timeout) {
	void *base = smap(sizeof(RobustSharedState));
	RobustSharedState *state = (RobustSharedState *)base;
	state->lock = ROBUST_LOCK_INIT;
	state->value = 0;

	if (two()) {
		struct timespec start, now;
		clock_gettime(CLOCK_MONOTONIC, &start);
		while (ALOAD(&state->value) != 1) {
			RobustGuard rg = robust_lock(&state->lock);
			clock_gettime(CLOCK_MONOTONIC, &now);
			if ((now.tv_sec - start.tv_sec) > 2) {	// 2s timeout
				cr_assert(0, "Timeout waiting for value == 1");
			}
		}
		cr_assert_eq(ALOAD(&state->value), 1);
	} else {
		RobustGuardImpl rg = robust_lock(&state->lock);
		AADD(&state->value, 1);
		exit(0);
	}
}

Test(core, robust_performance) {
	void *base = smap(sizeof(RobustSharedState));
	RobustSharedState *state = (RobustSharedState *)base;
	state->lock = ROBUST_LOCK_INIT;
	state->value = 0;
	struct timespec start, end;
	const int N = 100000;

	// Non-contended
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < N; i++) {
		RobustGuard rg = robust_lock(&state->lock);
		AADD(&state->value, 1);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	double non_contended_ns =
	    (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);

	/*
	printf("Non-contended: %.2f ns per lock+unlock\n",
	       non_contended_ns / N);
	       */

	cr_assert_eq(ALOAD(&state->value), N);

	// Contended (two processes)
	state->value = 0;
	if (two()) {
		for (int i = 0; i < N / 2; i++) {
			RobustGuard rg = robust_lock(&state->lock);
			AADD(&state->value, 1);
		}
		wait(NULL);
	} else {
		for (int i = 0; i < N / 2; i++) {
			RobustGuard rg = robust_lock(&state->lock);
			AADD(&state->value, 1);
		}
		exit(0);
	}
	cr_assert_eq(ALOAD(&state->value), N);
}

/*
Test(core, robust_timeout2) {
	void *base = smap(sizeof(RobustSharedState));
	RobustSharedState *state = (RobustSharedState *)base;
	{
		RobustGuard rg = robust_lock(&state->lock);
		AADD(&state->value, 1);
	}

	sleepm(20000);
	sleepm(3000);
	{
		RobustGuard rg = robust_lock(&state->lock);
		AADD(&state->value, 1);
	}
	sleepm(10000);
}
*/
