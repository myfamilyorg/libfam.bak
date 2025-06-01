#define _GNU_SOURCE
#include <errno.h>
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
		errno = -result;
		return -1;
	}
	return result;
}
#endif /* __linux__ */

void sig_ign(int __attribute((unused)) sig) {}

void (*next_task)(int);

/* Signal handler for SIGALRM */
void timeout_handler(int v) { next_task(v); }

static void __attribute__((constructor)) _disable_sigpipe__(void) {
#ifdef __linux__
	struct kernel_sigaction act = {0};
	struct kernel_sigaction act2 = {0};
	act.k_sa_handler = sig_ign;
	act.k_sa_flags = SA_RESTORER;
	act.k_sa_restorer = sigaction_restorer;
	if (syscall_sigaction_rt(SIGPIPE, &act, NULL, 8) < 0) {
		const char *msg = "WARNL could not register SIGPIPE handler\n";
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
		const char *msg = "WARNL could not register SIGPIPE handler\n";
		write(2, msg, strlen(msg));
	}
	act2.sa_handler = timeout_handler;
	act2.sa_flags = 0;
	sigaction(SIGALRM, &act2, NULL);
#endif
}

/* Timeout function */
int timeout(void (*task)(int), uint64_t milliseconds) {
	struct itimerval new_value;
	new_value.it_interval.tv_sec = 0;
	new_value.it_interval.tv_usec = 0;
	new_value.it_value.tv_sec = (long)(milliseconds / 1000);
	new_value.it_value.tv_usec = (long)((milliseconds % 1000) * 1000);

	next_task = task;

	if (setitimer(ITIMER_REAL, &new_value, NULL) < 0) {
		return -1;
	}

	return 0;
}
