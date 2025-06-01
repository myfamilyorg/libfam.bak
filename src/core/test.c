#include <criterion/criterion.h>
#include <lock.h>
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
			LockGuard lg1 = lock_read(&state->lock1);
			if (state->value1) break;
		}
		cr_assert_eq(state->value3++, 1);
		LockGuard lg2 = lock_read(&state->lock2);
		cr_assert_eq(state->value2, 1);
		cr_assert_eq(state->value3++, 3);
	} else {
		{
			LockGuard lg2 = lock_write(&state->lock2);
			cr_assert_eq(state->value3++, 0);
			{
				LockGuard lg1 = lock_write(&state->lock1);
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
			LockGuard lg1 = lock_read(&state->lock1);
			if (state->value1) break;
		}
		cr_assert_eq(state->value3++, 1);
		LockGuard lg2 = lock_write(&state->lock2);
		cr_assert_eq(state->value2, 1);
		cr_assert_eq(state->value3++, 3);
	} else {
		{
			LockGuard lg2 = lock_write(&state->lock2);
			cr_assert_eq(state->value3++, 0);
			{
				LockGuard lg1 = lock_write(&state->lock1);
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
			LockGuard lg1 = lock_read(&state->lock1);
			if (state->value1 == 2) break;
		}
		LockGuard lg2 = lock_write(&state->lock2);
		cr_assert_eq(state->value2, 2);
	} else {
		if (two()) {
			LockGuard lg2 = lock_read(&state->lock2);
			state->value1++;
			sleepm(10);
			state->value2++;
		} else {
			LockGuard lg2 = lock_read(&state->lock2);
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
		// Parent (Writer)
		// Wait for both children to acquire lock2
		while (true) {
			LockGuard lg1 = lock_read(&state->lock1);
			if (state->value1 == 2) break;
		}

		{
			// parent first to request write lock because children
			// sleep 100 and 200ms respectively. But doesn't acquire
			// lock until both release their read locks
			// this shows write starvation prevention
			LockGuard lg2 = lock_write(&state->lock2);
			cr_assert_eq(state->value2++,
				     0);  // First to set value2 to 1

			cr_assert_eq(state->value3, 0);
		}

		// ensure that child1 succeeds and is second to update value3
		while (!ALOAD(&state->value3));

	} else {
		if (two()) {
			{
				LockGuard lg2 = lock_read(&state->lock2);
				AADD(&state->value1, 1);
				sleepm(100);
			}
			{
				// lg2 will request read lock after writer, but
				// get write after
				LockGuard lg2 = lock_read(&state->lock2);
				cr_assert_eq(state->value2++, 1);
				AADD(&state->value3, 1);
			}
			exit(0);
		} else {
			{
				// hold lock to ensure ordering of writer/reader
				LockGuard lg2 = lock_read(&state->lock2);
				AADD(&state->value1, 1);
				sleepm(200);
			}
			exit(0);
		}
	}
}

