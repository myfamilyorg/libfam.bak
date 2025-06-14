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

#include <atomic.H>
#include <limits.H>
#include <lock.H>
#include <misc.H>
#include <sys.H>
#include <syscall.H>
#include <syscall_const.H>

#define WFLAG (0x1 << 31)
#define WREQUEST (0x1 << 30)

void lockguard_cleanup(LockGuardImpl *lg) {
	if (lg->is_write) {
		ASTORE(lg->lock, 0);
		futex(lg->lock, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
	} else {
		uint32_t v = __sub32(lg->lock, 1);
		if ((v & ~(WREQUEST | WFLAG)) == 1)
			futex(lg->lock, FUTEX_WAKE, 1, NULL, NULL, 0);
	}
}

LockGuardImpl rlock(Lock *lock) {
	LockGuardImpl ret = {NULL, false};
	ret.lock = lock;
	while (true) {
		uint32_t cur = ALOAD(lock);
		if ((cur & (WREQUEST | WFLAG)) == 0) {
			if (__cas32(lock, &cur, cur + 1)) break;
		} else
			futex(lock, FUTEX_WAIT, cur, NULL, NULL, 0);
	}
	return ret;
}

LockGuardImpl wlock(Lock *lock) {
	LockGuardImpl ret = {NULL, true};
	ret.lock = lock;
	while (true) {
		uint32_t cur = ALOAD(lock);
		if ((cur & ~WREQUEST) == 0) {
			if (__cas32(lock, &cur, WFLAG)) break;
		} else {
			if ((cur & WREQUEST) == 0) {
				int32_t desired = cur | WREQUEST;
				if (!__cas32(lock, &cur, desired)) continue;
			}
			futex(lock, FUTEX_WAIT, cur | WREQUEST, NULL, NULL, 0);
		}
	}
	return ret;
}
