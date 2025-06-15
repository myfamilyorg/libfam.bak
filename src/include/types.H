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
typedef int int32_t;
typedef long int64_t;
typedef __int128_t int128_t;

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
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
	uint32_t u32;
	uint64_t u64;
} epoll_data_t;

struct epoll_event {
	uint32_t events;
	epoll_data_t data;
}
#ifdef __amd64__
__attribute__((packed))
#endif /* packed on __amd64__ */
;

struct clone_args {
	uint64_t flags;
	uint64_t pidfd;
	uint64_t child_tid;
	uint64_t parent_tid;
	uint64_t exit_signal;
	uint64_t stack;
	uint64_t stack_size;
	uint64_t tls;
	uint64_t set_tid;
	uint64_t set_tid_size;
};

struct rt_sigaction {
	void (*k_sa_handler)(int);
	uint64_t k_sa_flags;
	void (*k_sa_restorer)(void);
	uint64_t k_sa_mask;
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

typedef uint32_t socklen_t;

struct sockaddr_in {
        unsigned short sin_family;
        unsigned short sin_port;
        uint32_t sin_addr;
        unsigned char sin_zero[8];
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

typedef struct {
	int si_pid;
	int si_code;
	int si_status;
	char pad[128 - 3 * sizeof(int)];
} siginfo_t;

typedef struct {
	uint64_t bits[_NSIG / (8 * sizeof(uint64_t))];
} sigset_t;

#endif /* _TYPES_H__ */
