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
#include <error.H>
#include <format.H>
#include <misc.H>
#include <robust.H>
#include <sys.H>
#include <syscall.H>
#include <types.H>

RobustGuard robust_lock(RobustLock *lock) {
	RobustGuardImpl ret;
	uint32_t expected = 0;
	pid_t pid = getpid();

	while (!__cas32(lock, &expected, pid)) {
		if (kill(expected, 0) == -1 && err == ESRCH)
			if (__cas32(lock, &expected, pid)) break;
		expected = 0;
		yield();
	}

	ret.lock = lock;
	return ret;
}

void robustguard_cleanup(RobustGuardImpl *lg) {
	pid_t pid = getpid();
	if (!__cas32(lg->lock, (uint32_t *)&pid, 0)) {
		printf("lock state was %i. Our pid is %i\n", pid, getpid());
		panic("unxpected lock state!");
	}
}
