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

#ifndef _TYPES_H
#define _TYPES_H

#define PUBLIC __attribute__((visibility("default")))

#ifndef NULL
#define NULL ((void *)0)
#endif /* NULL */

typedef signed char i8;
typedef short int i16;
typedef int i32;
typedef long i64;
typedef __int128_t i128;

typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned int u32;
typedef unsigned long u64;
typedef __uint128_t u128;

#ifndef bool
#define bool u8
#endif

#ifndef false
#define false (bool)0
#endif

#ifndef true
#define true (bool)1
#endif

#ifndef _NSIG
#define _NSIG 64
#endif

struct iovec {
	void *iov_base;
	u64 iov_len;
};

struct flock {
	i16 l_type;
	i16 l_whence;
	i64 l_start;
	i64 l_len;
	i32 l_pid;
};

typedef union epoll_data {
	void *ptr;
	i32 fd;
	u32 u32;
	u64 u64;
} epoll_data_t;

struct epoll_event {
	u32 events;
	epoll_data_t data;
}
#ifdef __amd64__
__attribute__((packed))
#endif /* packed on __amd64__ */
;

struct clone_args {
	u64 flags;
	u64 pidfd;
	u64 child_tid;
	u64 parent_tid;
	u64 exit_signal;
	u64 stack;
	u64 stack_size;
	u64 tls;
	u64 set_tid;
	u64 set_tid_size;
};

struct rt_sigaction {
	void (*k_sa_handler)(i32);
	u64 k_sa_flags;
	void (*k_sa_restorer)(void);
	u64 k_sa_mask;
};

struct sockaddr {
	unsigned short sa_family;
	u8 sa_data[14];
};

typedef unsigned short sa_family_t;

struct sockaddr_un {
	sa_family_t sun_family;
	u8 sun_path[108];
};

typedef u32 socklen_t;

struct sockaddr_in {
	unsigned short sin_family;
	unsigned short sin_port;
	u32 sin_addr;
	u8 sin_zero[8];
};

struct timespec {
	u64 tv_sec;
	u64 tv_nsec;
};

struct timezone {
	i32 tz_minuteswest;
	i32 tz_dsttime;
};

struct timeval {
	u64 tv_sec;
	u64 tv_usec;
};

struct itimerval {
	struct timeval it_interval;
	struct timeval it_value;
};

#define __SI_MAX_SIZE 128
#define __SI_PAD_SIZE ((__SI_MAX_SIZE / sizeof(i32)) - 4)
typedef u32 __uid_t;
typedef i64 __clock_t;

union sigval {
	i32 sival_int;
	void *sival_ptr;
};
typedef union sigval __sigval_t;

typedef struct {
	i32 si_signo; /* Signal number.  */
	i32 si_errno; /* If non-zero, an errno value associated with
			 this signal, as defined in <errno.h>.  */
	i32 si_code;  /* Signal code.  */
	i32 __pad0;   /* Explicit padding.  */
	union {
		i32 _pad[__SI_PAD_SIZE];

		/* kill().  */
		struct {
			i32 si_pid;	/* Sending process ID.  */
			__uid_t si_uid; /* Real user ID of sending process.  */
		} _kill;

		/* POSIX.1b timers.  */
		struct {
			i32 si_tid;	      /* Timer ID.  */
			i32 si_overrun;	      /* Overrun count.  */
			__sigval_t si_sigval; /* Signal value.  */
		} _timer;

		/* POSIX.1b signals.  */
		struct {
			i32 si_pid;	/* Sending process ID.  */
			__uid_t si_uid; /* Real user ID of sending process.  */
			__sigval_t si_sigval; /* Signal value.  */
		} _rt;

		/* SIGCHLD.  */
		struct {
			i32 si_pid;	/* Which child.	 */
			__uid_t si_uid; /* Real user ID of sending process.  */
			i32 si_status;	/* Exit value or signal.  */
			__clock_t si_utime;
			__clock_t si_stime;
		} _sigchld;

		/* SIGILL, SIGFPE, SIGSEGV, SIGBUS.  */
		struct {
			void *si_addr;	 /* Faulting insn/memory ref.  */
			i16 si_addr_lsb; /* Valid LSB of the reported
						  address.  */
			union {
				/* used when si_code=SEGV_BNDERR */
				struct {
					void *_lower;
					void *_upper;
				} _addr_bnd;
				/* used when si_code=SEGV_PKUERR */
				u32 _pkey;
			} _bounds;
		} _sigfault;

		/* SIGPOLL.  */
		struct {
			i64 si_band; /* Band event for SIGPOLL.  */
			i32 si_fd;
		} _sigpoll;

		/* SIGSYS.  */
		struct {
			void *_call_addr; /* Calling user insn.  */
			i32 _syscall;	  /* Triggering system call number.  */
			u32 _arch;	  /* AUDIT_ARCH_* of syscall.  */
		} _sigsys;
	} _sifields;
} siginfo_t;

typedef struct {
	u64 bits[_NSIG / (8 * sizeof(u64))];
} sigset_t;

#endif /* _TYPES_H */
