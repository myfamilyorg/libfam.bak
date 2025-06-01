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
#include <lock.h>
#include <misc.h>
#include <sys.h>

#define WFLAG (0x1UL << 63)
#define WREQUEST (0x1UL << 62)

void lockguard_cleanup(LockGuardImpl *lg) {
	if (!lg->lock) {
		const char *msg =
		    "lock error: Illegal state LockGuard had a null lock value";
		int v = write(2, msg, strlen(msg));
		if (v) {
		}
		exit(-1);
	}
	if (lg->is_write) {
		uint64_t wflag = WFLAG;
		if (!CAS(lg->lock, &wflag, 0)) {
			const char *msg =
			    "lock error: tried to write unlock a lock that is "
			    "not a write lock!";
			int v = write(2, msg, strlen(msg));
			if (v) {
			}
			exit(-1);
		}
	} else {
		if ((ALOAD(lg->lock) & WFLAG) != 0) {
			const char *msg =
			    "lock error: tried to read unlock a lock that is "
			    "not a read lock!";
			int v = write(2, msg, strlen(msg));
			if (v) {
			}
			exit(-1);
		}
		ASUB(lg->lock, 1);
	}
}

LockGuardImpl lock_read(Lock *lock) {
	uint64_t state, desired;
	LockGuardImpl ret;
	do {
		state = ALOAD(lock) & ~(WFLAG | WREQUEST);
		desired = state + 1;
	} while (!CAS(lock, &state, desired));
	ret.lock = lock;
	ret.is_write = false;
	return ret;
}

LockGuardImpl lock_write(Lock *lock) {
	uint64_t state, desired;
	LockGuardImpl ret;
	do {
		state = ALOAD(lock) & ~(WFLAG | WREQUEST);
		if (state == 0)
			desired = WFLAG;
		else
			desired = state | WREQUEST;
	} while (!CAS(lock, &state, desired));

	if (desired != WFLAG) {
		desired = WFLAG;
	start_loop:
		do {
			state = ALOAD(lock);
			if (state != WREQUEST) goto start_loop;
		} while (!CAS(lock, &state, desired));
	}
	ret.lock = lock;
	ret.is_write = true;
	return ret;
}
