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

#ifndef NULL
#define NULL ((void *)0)
#endif /* NULL */

typedef signed char int8_t;
typedef short int int16_t;
typedef int i32;
typedef long i64;
typedef __int128_t int128_t;

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int u32;
typedef unsigned long u64;
typedef __uint128_t uint128_t;

#ifndef bool
#define bool uint8_t
#endif

#ifndef false
#define false (bool)0
#endif

#ifndef true
#define true (bool)1
#endif

#define _NSIG 64

typedef union epoll_data {
	void *ptr;
	int fd;
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
	void (*k_sa_handler)(int);
	u64 k_sa_flags;
	void (*k_sa_restorer)(void);
	u64 k_sa_mask;
};

struct sockaddr {
	unsigned short sa_family;
	char sa_data[14];
};

typedef unsigned short sa_family_t;

struct sockaddr_un {
	sa_family_t sun_family;
	char sun_path[108];
};

typedef u32 socklen_t;

struct sockaddr_in {
        unsigned short sin_family;
        unsigned short sin_port;
        u32 sin_addr;
        unsigned char sin_zero[8];
};

struct timespec {
	u64 tv_sec;
	u64 tv_nsec;
};

struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};

struct timeval {
	u64 tv_sec;
	u64 tv_usec;
};

struct itimerval {
	struct timeval it_interval;
	struct timeval it_value;
};

typedef struct {
	int si_pid;
	int si_code;
	int si_status;
	char pad[128 - 3 * sizeof(int)];
} siginfo_t;

typedef struct {
	u64 bits[_NSIG / (8 * sizeof(u64))];
} sigset_t;

#endif /* _TYPES_H__ */
