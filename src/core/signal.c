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

#include <error.H>
#include <format.H>
#include <limits.H>
#include <misc.H>
#include <sys.H>
#include <syscall_const.H>

bool _debug_set_timeout_fail = false;

STATIC void sig_ign(i32 __attribute((unused)) sig) {}

typedef struct {
	void (*task)(void);
	u64 exec_millis;
} TaskEntry;

#define MAX_TASKS 32
static TaskEntry pending_tasks[MAX_TASKS];
STATIC i32 cur_tasks = 0;

STATIC i32 set_next_timer(u64 now) {
	i32 i;
	u64 next_task_time = SIZE_MAX;

#if TEST == 1
	if (_debug_set_timeout_fail) return -1;
#endif /* TEST */

	for (i = 0; i < cur_tasks; i++) {
		if (pending_tasks[i].exec_millis < next_task_time)
			next_task_time = pending_tasks[i].exec_millis;
	};

	if (next_task_time != SIZE_MAX) {
		struct itimerval new_value = {0};
		u64 delay_ms =
		    (next_task_time > now) ? (next_task_time - now) : 1;
		new_value.it_value.tv_sec = (i64)(delay_ms / 1000);
		new_value.it_value.tv_usec = (i64)((delay_ms % 1000) * 1000);
		if (setitimer(ITIMER_REAL, &new_value, NULL) < 0) return -1;
	}

	return 0;
}

STATIC void timeout_handler(i32 __attribute__((unused)) v) {
	TaskEntry rem_tasks[MAX_TASKS];
	i32 rem_task_count = 0;
	u64 now = micros() / 1000;
	i32 i;
	for (i = 0; i < cur_tasks; i++) {
		if (now >= pending_tasks[i].exec_millis) {
			pending_tasks[i].task();
		} else {
			rem_tasks[rem_task_count++] = pending_tasks[i];
		}
	}
	cur_tasks = rem_task_count;
	for (i = 0; i < cur_tasks; i++) pending_tasks[i] = rem_tasks[i];
	set_next_timer(now);
}

void __attribute__((constructor)) signals_init(void) {
	struct rt_sigaction act = {0};
	struct rt_sigaction act2 = {0};
	act.k_sa_handler = sig_ign;
	act.k_sa_flags = SA_RESTORER;
	act.k_sa_restorer = restorer;
	if (rt_sigaction(SIGPIPE, &act, NULL, 8) < 0 ||
	    _debug_set_timeout_fail) {
		const u8 *msg = "WARN: could not register SIGPIPE handler\n";
		i32 __attribute__((unused)) v = write(2, msg, strlen(msg));
	}
	act2.k_sa_handler = timeout_handler;
	act2.k_sa_flags = SA_RESTORER;
	act2.k_sa_restorer = restorer;
	rt_sigaction(SIGALRM, &act2, NULL, 8);
	cur_tasks = 0;
}
i32 timeout(void (*task)(void), u64 milliseconds) {
	u64 now = micros() / 1000;
	i32 ret;

	if (!task || milliseconds == 0) {
		err = EINVAL;
		return -1;
	}

	if (cur_tasks == MAX_TASKS) {
		err = EOVERFLOW;
		return -1;
	}

	pending_tasks[cur_tasks].task = task;
	pending_tasks[cur_tasks].exec_millis = now + milliseconds;
	cur_tasks++;
	ret = set_next_timer(now);
	if (ret == -1) {
		cur_tasks--;
	}
	return ret;
}
