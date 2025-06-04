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

#include <atomic.h>
#include <limits.h>
#include <lock.h>
#include <misc.h>
#include <sys.h>
#include <syscall.h>
#include <syscall_const.h>

#define WFLAG ((uint64_t)1 << 63)
#define WREQUEST ((uint64_t)1 << 62)
#define WAITER_MASK ((uint64_t)0x3FFFFFFF << 32)
#define READER_MASK ((uint64_t)0xFFFFFFFF)

#define WAITER_ADDR(lock) ((uint32_t *)((char *)(lock) + 4))
#define GET_WAITERS(state) ((uint32_t)((state >> 32) & 0x3FFFFFFF))

void lockguard_cleanup(LockGuardImpl *lg) {
	uint64_t state;
	if (!lg->lock)
		panic(
		    "lock error: Illegal state LockGuard had a null lock "
		    "value");

	if (lg->is_write) {
		uint64_t wflag = WFLAG;
		if (!CAS(lg->lock, &wflag, 0))
			panic(
			    "lock error: tried to write unlock a lock that is "
			    "not a write lock!");
		if (GET_WAITERS(ALOAD(lg->lock)) > 0)
			futux(WAITER_ADDR(lg->lock), FUTEX_WAKE, INT_MAX, NULL,
			      NULL, 0);
	} else {
		if ((ALOAD(lg->lock) & WFLAG) != 0)
			panic(
			    "lock error: tried to read unlock a lock that is "
			    "not a read lock!");
		ASUB(lg->lock, 1);
		state = ALOAD(lg->lock);
		if ((state & READER_MASK) == 0 && (state & WREQUEST) &&
		    GET_WAITERS(state) > 0)
			futux(WAITER_ADDR(lg->lock), FUTEX_WAKE, 1, NULL, NULL,
			      0);
	}
}

LockGuardImpl rlock(Lock *lock) {
	uint64_t state, desired;
	LockGuardImpl ret;
	ret.lock = lock;
	ret.is_write = false;

	while (true) {
		state = ALOAD(lock);
		if ((state & (WFLAG | WREQUEST)) == 0) {
			desired = state + 1;
			if (CAS(lock, &state, desired)) return ret;
		} else {
			uint32_t waiter_val = GET_WAITERS(state);
			AADD(lock, ((uint64_t)1 << 32));
			state = ALOAD(lock);
			if (state & (WFLAG | WREQUEST)) {
				futux(WAITER_ADDR(lock), FUTEX_WAIT,
				      waiter_val + 1, NULL, NULL, 0);
			}
			ASUB(lock, ((uint64_t)1 << 32));
		}
	}
}

LockGuardImpl wlock(Lock *lock) {
	uint64_t state, desired;
	LockGuardImpl ret;
	ret.lock = lock;
	ret.is_write = true;

	while (true) {
		state = ALOAD(lock);
		if ((state & READER_MASK) == 0 &&
		    (state & (WFLAG | WREQUEST)) == 0) {
			desired = WFLAG;
			if (CAS(lock, &state, desired)) return ret;
		} else {
			desired = state | WREQUEST;
			if (CAS(lock, &state, desired)) break;
		}
	}

	while (true) {
		state = ALOAD(lock);
		if (state != WREQUEST) {
			uint32_t waiter_val = GET_WAITERS(state);
			AADD(lock, ((uint64_t)1 << 32));
			state = ALOAD(lock);
			if (state != WREQUEST) {
				futux(WAITER_ADDR(lock), FUTEX_WAIT,
				      waiter_val + 1, NULL, NULL, 0);
			}
			ASUB(lock, ((uint64_t)1 << 32));
			continue;
		}
		desired = WFLAG;
		if (CAS(lock, &state, desired)) return ret;
	}
}
