#define _GNU_SOURCE
#include <error.h>
#include <misc.h>
#include <signal.h>
#include <stdio.h>
#include <sys.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#ifdef __linux__
int setitimer(__itimer_which_t which, const struct itimerval *new_value,
	      struct itimerval *old_value);
#endif /* __linux__ */
#ifdef __APPLE__
int setitimer(int which, const struct itimerval *new_value,
	      struct itimerval *old_value);
#endif /* __APPLE__ */

#ifdef __linux__
#ifndef SA_RESTORER
#define SA_RESTORER 0x04000000
#endif

extern void sigaction_restorer(void);

struct kernel_sigaction {
	void (*k_sa_handler)(int);
	unsigned long k_sa_flags;
	void (*k_sa_restorer)(void);
	unsigned long k_sa_mask;
};

static int syscall_sigaction_rt(int signum, const struct kernel_sigaction *act,
				struct kernel_sigaction *oldact,
				size_t sigsetsize) {
	long result;
	__asm__ volatile(
	    "movq $13, %%rax\n"
	    "movq %1, %%rdi\n"
	    "movq %2, %%rsi\n"
	    "movq %3, %%rdx\n"
	    "movq %4, %%r10\n"
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)
	    : "r"((long)(signum)), "r"((long)(act)), "r"((long)(oldact)),
	      "r"((long)(sigsetsize))
	    : "rax", "rcx", "r11", "rdi", "rsi", "rdx", "r10", "memory");
	if (result < 0) {
		err = -result;
		return -1;
	}
	return result;
}
#endif /* __linux__ */

static void sig_ign(int __attribute((unused)) sig) {}

typedef struct {
	void (*task)(void);
	uint64_t exec_millis;
} TaskEntry;

#define MAX_TASKS 32
STATIC TaskEntry pending_tasks[MAX_TASKS];
STATIC int cur_tasks = 0;

STATIC int set_next_timer(uint64_t now) {
	int i;
	uint64_t next_task_time = UINT64_MAX;

	for (i = 0; i < cur_tasks; i++) {
		if (pending_tasks[i].exec_millis < next_task_time)
			next_task_time = pending_tasks[i].exec_millis;
	};

	if (next_task_time != UINT64_MAX) {
		struct itimerval new_value = {0};
		uint64_t delay_ms =
		    (next_task_time > now) ? (next_task_time - now) : 1;
		new_value.it_value.tv_sec = (long)(delay_ms / 1000);
		new_value.it_value.tv_usec = (long)((delay_ms % 1000) * 1000);

		if (setitimer(ITIMER_REAL, &new_value, NULL) < 0) return -1;
	}

	return 0;
}

STATIC void timeout_handler(int __attribute__((unused)) v) {
	TaskEntry rem_tasks[MAX_TASKS];
	int rem_task_count = 0;
	uint64_t now = micros() / 1000;
	int i;
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

static void __attribute__((constructor)) _setup_signals__(void) {
#ifdef __linux__
	struct kernel_sigaction act = {0};
	struct kernel_sigaction act2 = {0};
	act.k_sa_handler = sig_ign;
	act.k_sa_flags = SA_RESTORER;
	act.k_sa_restorer = sigaction_restorer;
	if (syscall_sigaction_rt(SIGPIPE, &act, NULL, 8) < 0) {
		const char *msg = "WARN: could not register SIGPIPE handler\n";
		int __attribute__((unused)) v = write(2, msg, strlen(msg));
	}
	act2.k_sa_handler = timeout_handler;
	act2.k_sa_flags = SA_RESTORER;
	act2.k_sa_restorer = sigaction_restorer;
	syscall_sigaction_rt(SIGALRM, &act2, NULL, 8);
#endif /* __linux__ */
#ifdef __APPLE__
	struct sigaction act = {0};
	struct sigaction act2 = {0};
	act.sa_handler = sig_ign;
	act.sa_flags = 0;
	if (sigaction(SIGPIPE, &act, NULL) < 0) {
		const char *msg = "WARN: could not register SIGPIPE handler\n";
		write(2, msg, strlen(msg));
	}
	act2.sa_handler = timeout_handler;
	act2.sa_flags = 0;
	sigaction(SIGALRM, &act2, NULL);
#endif
	cur_tasks = 0;
}

/* Timeout function */
int timeout(void (*task)(void), uint64_t milliseconds) {
	uint64_t now = micros() / 1000;
	int ret;

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
