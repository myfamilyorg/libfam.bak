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

#ifndef _TYPES_H__
#define _TYPES_H__

typedef union epoll_data {
	void *ptr;
	int fd;
	unsigned int u32;
	unsigned long u64;
} epoll_data_t;

struct epoll_event {
	unsigned int events;
	epoll_data_t data;
} __attribute__((packed));

struct clone_args {
	unsigned long flags;
	unsigned long pidfd;
	unsigned long child_tid;
	unsigned long parent_tid;
	unsigned long exit_signal;
	unsigned long stack;
	unsigned long stack_size;
	unsigned long tls;
	unsigned long set_tid;
	unsigned long set_tid_size;
};

struct rt_sigaction {
	void (*k_sa_handler)(int);
	unsigned long k_sa_flags;
	void (*k_sa_restorer)(void);
	unsigned long k_sa_mask;
};

struct sockaddr {
	unsigned short sa_family;
	char sa_data[14];
};

struct timespec {
	long tv_sec;
	long tv_nsec;
};

struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};

struct timeval {
	long tv_sec;
	long tv_usec;
};

struct itimerval {
	struct timeval it_interval;
	struct timeval it_value;
};

typedef int __itimer_which_t;

#ifndef NULL
#define NULL ((void *)0)
#endif /* NULL */

typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long int64_t;
typedef __int128_t int128_t;

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef __uint128_t uint128_t;

typedef long off_t;
typedef unsigned int mode_t;

typedef int32_t pid_t;
typedef unsigned long size_t;
typedef long ssize_t;

#ifndef bool
#define bool uint8_t
#endif

#ifndef false
#define false (bool)0
#endif

#ifndef true
#define true (bool)1
#endif

#endif /* _TYPES_H__ */
