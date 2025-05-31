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
	if (!lg->lock) return;
	if (lg->is_write) {
		if (ALOAD(lg->lock) != WFLAG) {
			const char *msg =
			    "lock error: tried to write unlock a lock that is "
			    "not a write lock!";
			write(2, msg, strlen(msg));
			exit(-1);
		}
		ASTORE(lg->lock, 0);
	} else {
		if ((ALOAD(lg->lock) & WFLAG) != 0) {
			const char *msg =
			    "lock error: tried to read unlock a lock that is "
			    "not a read lock!";
			write(2, msg, strlen(msg));
			exit(-1);
		}
		ASUB(lg->lock, 1);
	}
}

LockGuardImpl lock_read(Lock *lock) {
	uint64_t state, desired, do_yield = 0;
	LockGuardImpl ret;
	do {
		if (do_yield++) yield();
		state = ALOAD(lock) & ~(WFLAG | WREQUEST);
		desired = state + 1;
	} while (!CAS(lock, &state, desired));
	ret.lock = lock;
	ret.is_write = 0;
	return ret;
}

LockGuardImpl lock_write(Lock *lock) {
	uint64_t state, desired, do_yield = 0;
	LockGuardImpl ret;
	do {
		if (do_yield++) yield();
		state = ALOAD(lock) & ~(WFLAG | WREQUEST);
		desired = state | WREQUEST;
	} while (!CAS(lock, &state, desired));

	desired = WFLAG;
start_loop:
	do {
		state = ALOAD(lock);
		if (state != WREQUEST) {
			yield();
			goto start_loop;
		}
	} while (!CAS(lock, &state, desired));
	ret.lock = lock;
	ret.is_write = 1;
	return ret;
}
