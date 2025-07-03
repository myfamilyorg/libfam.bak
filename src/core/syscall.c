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
#include <init.H>
#include <sys.H>
#include <syscall_const.H>
#include <types.H>

bool _debug_no_write = false;
bool _debug_no_exit = false;
bool _debug_fail_getsockbyname = false;
bool _debug_fail_pipe2 = false;
bool _debug_fail_listen = false;
bool _debug_fail_setsockopt = false;
bool _debug_fail_fcntl = false;
bool _debug_fail_epoll_create1 = false;
bool _debug_fail_clone3 = false;

#define SET_ERR_VALUE       \
	if (ret < 0) {      \
		err = -ret; \
		return -1;  \
	}

#define SET_ERR       \
	SET_ERR_VALUE \
	return ret;

#define SET_ERR_I64_VOID_PTR       \
	if ((i64)ret < 0) {        \
		err = -(i64)ret;   \
		return (void *)-1; \
	}                          \
	return ret;

#define SYSCALL_EXIT_COV \
	__gcov_dump();   \
	SYSCALL_EXIT

#ifdef __aarch64__
#define DEFINE_SYSCALL0(sysno, ret_type, name) \
	static ret_type syscall_##name(void) { \
		i64 result;                    \
		__asm__ volatile(              \
		    "mov x8, %1\n"             \
		    "svc #0\n"                 \
		    "mov %0, x0\n"             \
		    : "=r"(result)             \
		    : "r"((i64)(sysno))        \
		    : "x8", "x0", "memory");   \
		return (ret_type)result;       \
	}

#define DEFINE_SYSCALL1(sysno, ret_type, name, type1, arg1) \
	static ret_type syscall_##name(type1 arg1) {        \
		i64 result;                                 \
		__asm__ volatile(                           \
		    "mov x8, %1\n"                          \
		    "mov x0, %2\n"                          \
		    "svc #0\n"                              \
		    "mov %0, x0\n"                          \
		    : "=r"(result)                          \
		    : "r"((i64)(sysno)), "r"((i64)(arg1))   \
		    : "x8", "x0", "memory");                \
		return (ret_type)result;                    \
	}

#define DEFINE_SYSCALL2(sysno, ret_type, name, type1, arg1, type2, arg2)    \
	static ret_type syscall_##name(type1 arg1, type2 arg2) {            \
		i64 result;                                                 \
		__asm__ volatile(                                           \
		    "mov x8, %1\n"                                          \
		    "mov x0, %2\n"                                          \
		    "mov x1, %3\n"                                          \
		    "svc #0\n"                                              \
		    "mov %0, x0\n"                                          \
		    : "=r"(result)                                          \
		    : "r"((i64)(sysno)), "r"((i64)(arg1)), "r"((i64)(arg2)) \
		    : "x8", "x0", "x1", "memory");                          \
		return (ret_type)result;                                    \
	}

#define DEFINE_SYSCALL3(sysno, ret_type, name, type1, arg1, type2, arg2,     \
			type3, arg3)                                         \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3) { \
		i64 result;                                                  \
		__asm__ volatile(                                            \
		    "mov x8, %1\n"                                           \
		    "mov x0, %2\n"                                           \
		    "mov x1, %3\n"                                           \
		    "mov x2, %4\n"                                           \
		    "svc #0\n"                                               \
		    "mov %0, x0\n"                                           \
		    : "=r"(result)                                           \
		    : "r"((i64)(sysno)), "r"((i64)(arg1)), "r"((i64)(arg2)), \
		      "r"((i64)(arg3))                                       \
		    : "x8", "x0", "x1", "x2", "memory");                     \
		return (ret_type)result;                                     \
	}

#define DEFINE_SYSCALL4(sysno, ret_type, name, type1, arg1, type2, arg2,     \
			type3, arg3, type4, arg4)                            \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3,   \
				       type4 arg4) {                         \
		i64 result;                                                  \
		__asm__ volatile(                                            \
		    "mov x8, %1\n"                                           \
		    "mov x0, %2\n"                                           \
		    "mov x1, %3\n"                                           \
		    "mov x2, %4\n"                                           \
		    "mov x3, %5\n"                                           \
		    "svc #0\n"                                               \
		    "mov %0, x0\n"                                           \
		    : "=r"(result)                                           \
		    : "r"((i64)(sysno)), "r"((i64)(arg1)), "r"((i64)(arg2)), \
		      "r"((i64)(arg3)), "r"((i64)(arg4))                     \
		    : "x8", "x0", "x1", "x2", "x3", "memory");               \
		return (ret_type)result;                                     \
	}

#define DEFINE_SYSCALL5(sysno, ret_type, name, type1, arg1, type2, arg2,     \
			type3, arg3, type4, arg4, type5, arg5)               \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3,   \
				       type4 arg4, type5 arg5) {             \
		i64 result;                                                  \
		__asm__ volatile(                                            \
		    "mov x8, %1\n"                                           \
		    "mov x0, %2\n"                                           \
		    "mov x1, %3\n"                                           \
		    "mov x2, %4\n"                                           \
		    "mov x3, %5\n"                                           \
		    "mov x4, %6\n"                                           \
		    "svc #0\n"                                               \
		    "mov %0, x0\n"                                           \
		    : "=r"(result)                                           \
		    : "r"((i64)(sysno)), "r"((i64)(arg1)), "r"((i64)(arg2)), \
		      "r"((i64)(arg3)), "r"((i64)(arg4)), "r"((i64)(arg5))   \
		    : "x8", "x0", "x1", "x2", "x3", "x4", "memory");         \
		return (ret_type)result;                                     \
	}

#define DEFINE_SYSCALL6(sysno, ret_type, name, type1, arg1, type2, arg2,     \
			type3, arg3, type4, arg4, type5, arg5, type6, arg6)  \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3,   \
				       type4 arg4, type5 arg5, type6 arg6) { \
		i64 result;                                                  \
		__asm__ volatile(                                            \
		    "mov x8, %1\n"                                           \
		    "mov x0, %2\n"                                           \
		    "mov x1, %3\n"                                           \
		    "mov x2, %4\n"                                           \
		    "mov x3, %5\n"                                           \
		    "mov x4, %6\n"                                           \
		    "mov x5, %7\n"                                           \
		    "svc #0\n"                                               \
		    "mov %0, x0\n"                                           \
		    : "=r"(result)                                           \
		    : "r"((i64)(sysno)), "r"((i64)(arg1)), "r"((i64)(arg2)), \
		      "r"((i64)(arg3)), "r"((i64)(arg4)), "r"((i64)(arg5)),  \
		      "r"((i64)(arg6))                                       \
		    : "x8", "x0", "x1", "x2", "x3", "x4", "x5", "memory");   \
		return (ret_type)result;                                     \
	}

#define SYSCALL_EXIT                 \
	__asm__ volatile(            \
	    "mov x8, #93\n"          \
	    "mov x0, %0\n"           \
	    "svc #0\n"               \
	    :                        \
	    : "r"((i64)status)       \
	    : "x8", "x0", "memory"); \
	while (true) {               \
	}

#define SYSCALL_RESTORER     \
	__asm__ volatile(    \
	    "mov x8, #139\n" \
	    "svc #0\n" ::    \
		: "x8", "memory");

static i32 syscall_waitid(i32 idtype, i32 id, siginfo_t *infop, i32 options) {
	i64 result;
	__asm__ volatile(
	    "mov x8, #95\n"
	    "mov x0, %1\n"
	    "mov x1, %2\n"
	    "mov x2, %3\n"
	    "mov x3, %4\n"
	    "svc #0\n"
	    "mov %0, x0\n"
	    : "=r"(result)
	    : "r"((i64)idtype), "r"((i64)id), "r"((i64)infop), "r"((i64)options)
	    : "x8", "x0", "x1", "x2", "x3", "memory");
	return (i32)result;
}

#elif defined(__amd64__)

#define DEFINE_SYSCALL0(sysno, ret_type, name)                        \
	static ret_type syscall_##name(void) {                        \
		i64 result;                                           \
		__asm__ volatile("movq $" #sysno                      \
				 ", %%rax\n"                          \
				 "syscall\n"                          \
				 "movq %%rax, %0\n"                   \
				 : "=r"(result)                       \
				 :                                    \
				 : "%rax", "%rcx", "%r11", "memory"); \
		return (ret_type)result;                              \
	}

#define DEFINE_SYSCALL1(sysno, ret_type, name, type1, arg1)                   \
	static ret_type syscall_##name(type1 arg1) {                          \
		i64 result;                                                   \
		__asm__ volatile("movq $" #sysno                              \
				 ", %%rax\n"                                  \
				 "movq %1, %%rdi\n"                           \
				 "syscall\n"                                  \
				 "movq %%rax, %0\n"                           \
				 : "=r"(result)                               \
				 : "r"((i64)(arg1))                           \
				 : "%rax", "%rcx", "%r11", "%rdi", "memory"); \
		return (ret_type)result;                                      \
	}

#define DEFINE_SYSCALL2(sysno, ret_type, name, type1, arg1, type2, arg2)   \
	static ret_type syscall_##name(type1 arg1, type2 arg2) {           \
		i64 result;                                                \
		__asm__ volatile("movq $" #sysno                           \
				 ", %%rax\n"                               \
				 "movq %1, %%rdi\n"                        \
				 "movq %2, %%rsi\n"                        \
				 "syscall\n"                               \
				 "movq %%rax, %0\n"                        \
				 : "=r"(result)                            \
				 : "r"((i64)(arg1)), "r"((i64)(arg2))      \
				 : "%rax", "%rcx", "%r11", "%rdi", "%rsi", \
				   "memory");                              \
		return (ret_type)result;                                   \
	}

#define DEFINE_SYSCALL3(sysno, ret_type, name, type1, arg1, type2, arg2,     \
			type3, arg3)                                         \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3) { \
		i64 result;                                                  \
		__asm__ volatile(                                            \
		    "movq $" #sysno                                          \
		    ", %%rax\n"                                              \
		    "movq %1, %%rdi\n"                                       \
		    "movq %2, %%rsi\n"                                       \
		    "movq %3, %%rdx\n"                                       \
		    "syscall\n"                                              \
		    "movq %%rax, %0\n"                                       \
		    : "=r"(result)                                           \
		    : "r"((i64)(arg1)), "r"((i64)(arg2)), "r"((i64)(arg3))   \
		    : "%rax", "%rdi", "%rsi", "%rdx", "%rcx", "%r11",        \
		      "memory");                                             \
		return (ret_type)result;                                     \
	}

#define DEFINE_SYSCALL4(sysno, ret_type, name, type1, arg1, type2, arg2,   \
			type3, arg3, type4, arg4)                          \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3, \
				       type4 arg4) {                       \
		i64 result;                                                \
		__asm__ volatile("movq $" #sysno                           \
				 ", %%rax\n"                               \
				 "movq %1, %%rdi\n"                        \
				 "movq %2, %%rsi\n"                        \
				 "movq %3, %%rdx\n"                        \
				 "movq %4, %%r10\n"                        \
				 "syscall\n"                               \
				 "movq %%rax, %0\n"                        \
				 : "=r"(result)                            \
				 : "r"((i64)(arg1)), "r"((i64)(arg2)),     \
				   "r"((i64)(arg3)), "r"((i64)(arg4))      \
				 : "%rax", "%rcx", "%r11", "%rdi", "%rsi", \
				   "%rdx", "%r10", "memory");              \
		return (ret_type)result;                                   \
	}

#define DEFINE_SYSCALL5(sysno, ret_type, name, type1, arg1, type2, arg2,   \
			type3, arg3, type4, arg4, type5, arg5)             \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3, \
				       type4 arg4, type5 arg5) {           \
		i64 result;                                                \
		__asm__ volatile("movq $" #sysno                           \
				 ", %%rax\n"                               \
				 "movq %1, %%rdi\n"                        \
				 "movq %2, %%rsi\n"                        \
				 "movq %3, %%rdx\n"                        \
				 "movq %4, %%r10\n"                        \
				 "movq %5, %%r8\n"                         \
				 "syscall\n"                               \
				 "movq %%rax, %0\n"                        \
				 : "=r"(result)                            \
				 : "r"((i64)(arg1)), "r"((i64)(arg2)),     \
				   "r"((i64)(arg3)), "r"((i64)(arg4)),     \
				   "r"((i64)(arg5))                        \
				 : "%rax", "%rcx", "%r11", "%rdi", "%rsi", \
				   "%rdx", "%r10", "%r8", "memory");       \
		return (ret_type)result;                                   \
	}

#define DEFINE_SYSCALL6(sysno, ret_type, name, type1, arg1, type2, arg2,     \
			type3, arg3, type4, arg4, type5, arg5, type6, arg6)  \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3,   \
				       type4 arg4, type5 arg5, type6 arg6) { \
		i64 result;                                                  \
		__asm__ volatile("movq $" #sysno                             \
				 ", %%rax\n"                                 \
				 "movq %1, %%rdi\n"                          \
				 "movq %2, %%rsi\n"                          \
				 "movq %3, %%rdx\n"                          \
				 "movq %4, %%r10\n"                          \
				 "movq %5, %%r8\n"                           \
				 "movq %6, %%r9\n"                           \
				 "syscall\n"                                 \
				 "movq %%rax, %0\n"                          \
				 : "=r"(result)                              \
				 : "r"((i64)(arg1)), "r"((i64)(arg2)),       \
				   "r"((i64)(arg3)), "r"((i64)(arg4)),       \
				   "r"((i64)(arg5)), "r"((i64)(arg6))        \
				 : "%rax", "%rcx", "%r11", "%rdi", "%rsi",   \
				   "%rdx", "%r10", "%r8", "%r9", "memory");  \
		return (ret_type)result;                                     \
	}

#define SYSCALL_EXIT                                     \
	__asm__ volatile(                                \
	    "movq $60, %%rax\n"                          \
	    "movq %0, %%rdi\n"                           \
	    "syscall\n"                                  \
	    :                                            \
	    : "r"((i64)status)                           \
	    : "%rax", "%rdi", "%rcx", "%r11", "memory"); \
	while (true) {                                   \
	}

#define SYSCALL_RESTORER        \
	__asm__ volatile(       \
	    "movq $15, %%rax\n" \
	    "syscall\n"         \
	    :                   \
	    :                   \
	    : "%rax", "%rcx", "%r11", "memory");

static i32 syscall_waitid(i32 idtype, i32 id, siginfo_t *infop, i32 options) {
	i64 result;
	__asm__ volatile(
	    "movq $247, %%rax\n"
	    "movq %1, %%rdi\n"
	    "movq %2, %%rsi\n"
	    "movq %3, %%rdx\n"
	    "movq %4, %%r10\n"
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)
	    : "r"((i64)idtype), "r"((i64)id), "r"((i64)infop), "r"((i64)options)
	    : "rax", "rdi", "rsi", "rdx", "r10", "rcx", "r11", "memory");
	return (i32)result;
}

#endif /* Arch */

#ifdef __aarch64__
DEFINE_SYSCALL2(59, i32, pipe2, i32 *, fds, i32, flags)
DEFINE_SYSCALL3(35, i32, unlinkat, i32, dfd, const u8 *, path, i32, flag)
DEFINE_SYSCALL3(64, i64, write, i32, fd, const void *, buf, u64, count)
DEFINE_SYSCALL3(63, i64, read, i32, fd, void *, buf, u64, count)
DEFINE_SYSCALL2(215, i32, munmap, void *, addr, u64, len)
DEFINE_SYSCALL1(57, i32, close, i32, fd)
DEFINE_SYSCALL3(25, i32, fcntl, i32, fd, i32, cmd, i64, arg)
DEFINE_SYSCALL3(203, i32, connect, i32, sockfd, const struct sockaddr *, addr,
		u32, addrlen)
DEFINE_SYSCALL5(208, i32, setsockopt, i32, sockfd, i32, level, i32, optname,
		const void *, optval, u32, optlen)
DEFINE_SYSCALL5(209, i32, getsockopt, i32, sockfd, i32, level, i32, optname,
		void *, optval, u32 *, optlen)

DEFINE_SYSCALL3(200, i32, bind, i32, sockfd, const struct sockaddr *, addr, u32,
		addrlen)
DEFINE_SYSCALL2(201, i32, listen, i32, sockfd, i32, backlog)
DEFINE_SYSCALL3(204, i32, getsockname, i32, sockfd, struct sockaddr *, addr,
		u32 *, addrlen)
DEFINE_SYSCALL3(202, i32, accept, i32, sockfd, struct sockaddr *, addr, u32 *,
		addrlen)
DEFINE_SYSCALL2(210, i32, shutdown, i32, sockfd, i32, how)
DEFINE_SYSCALL3(198, i32, socket, i32, domain, i32, type, i32, protocol)
DEFINE_SYSCALL3(278, i32, getrandom, void *, buffer, u64, length, u32, flags)
DEFINE_SYSCALL6(222, void *, mmap, void *, addr, u64, length, i32, prot, i32,
		flags, i32, fd, i64, offset)
DEFINE_SYSCALL2(101, i32, nanosleep, const struct timespec *, req,
		struct timespec *, rem)
DEFINE_SYSCALL0(124, i32, sched_yield)
DEFINE_SYSCALL2(169, i32, gettimeofday, struct timeval *, tv, void *, tz)
DEFINE_SYSCALL2(170, i32, settimeofday, const struct timeval *, tv,
		const void *, tz)
DEFINE_SYSCALL1(20, i32, epoll_create1, i32, flags)
DEFINE_SYSCALL6(22, i32, epoll_pwait, i32, epfd, struct epoll_event *, events,
		i32, maxevents, i32, timeout, const sigset_t *, sigs, i32, size)
DEFINE_SYSCALL4(21, i32, epoll_ctl, i32, epfd, i32, op, i32, fd,
		struct epoll_event *, event)
DEFINE_SYSCALL4(56, i32, openat, i32, dfd, const u8 *, pathname, i32, flags,
		u32, mode)
DEFINE_SYSCALL3(62, i64, lseek, i32, fd, i64, offset, i32, whence)
DEFINE_SYSCALL1(83, i32, fdatasync, i32, fd)
DEFINE_SYSCALL2(46, i32, ftruncate, i32, fd, i64, length)
DEFINE_SYSCALL3(103, i32, setitimer, i32, which, const struct itimerval *,
		new_value, struct itimerval *, old_value)
DEFINE_SYSCALL2(435, i32, clone3, struct clone_args *, args, u64, size)
DEFINE_SYSCALL6(98, i64, futex, u32 *, uaddr, i32, futex_op, u32, val,
		const struct timespec *, timeout, u32 *, uaddr2, u32, val3)
DEFINE_SYSCALL4(134, i32, rt_sigaction, i32, signum,
		const struct rt_sigaction *, act, struct rt_sigaction *, oldact,
		u64, sigsetsize)
DEFINE_SYSCALL0(172, i32, getpid)
DEFINE_SYSCALL2(129, i32, kill, i32, pid, i32, signal)
#elif defined(__amd64__)
/* System call definitions */
DEFINE_SYSCALL2(293, i32, pipe2, i32 *, fds, i32, flags)
DEFINE_SYSCALL3(263, i32, unlinkat, i32, dfd, const u8 *, path, i32, flags)
DEFINE_SYSCALL3(1, i64, write, i32, fd, const void *, buf, u64, count)
DEFINE_SYSCALL3(0, i64, read, i32, fd, void *, buf, u64, count)
DEFINE_SYSCALL2(11, i32, munmap, void *, addr, u64, len)
DEFINE_SYSCALL1(3, i32, close, i32, fd)
DEFINE_SYSCALL3(72, i32, fcntl, i32, fd, i32, cmd, i64, arg)
DEFINE_SYSCALL3(42, i32, connect, i32, sockfd, const struct sockaddr *, addr,
		u32, addrlen)
DEFINE_SYSCALL5(54, i32, setsockopt, i32, sockfd, i32, level, i32, optname,
		const void *, optval, u32, optlen)
DEFINE_SYSCALL5(55, i32, getsockopt, i32, sockfd, i32, level, i32, optname,
		void *, optval, u32 *, optlen)
DEFINE_SYSCALL3(49, i32, bind, i32, sockfd, const struct sockaddr *, addr, u32,
		addrlen)
DEFINE_SYSCALL2(50, i32, listen, i32, sockfd, i32, backlog)
DEFINE_SYSCALL3(51, i32, getsockname, i32, sockfd, struct sockaddr *, addr,
		u32 *, addrlen)
DEFINE_SYSCALL3(43, i32, accept, i32, sockfd, struct sockaddr *, addr, u32 *,
		addrlen)
DEFINE_SYSCALL2(48, i32, shutdown, i32, sockfd, i32, how)
DEFINE_SYSCALL3(41, i32, socket, i32, domain, i32, type, i32, protocol)
DEFINE_SYSCALL3(318, i32, getrandom, void *, buffer, u64, length, u32, flags)
DEFINE_SYSCALL6(9, void *, mmap, void *, addr, u64, length, i32, prot, i32,
		flags, i32, fd, i64, offset)
DEFINE_SYSCALL2(35, i32, nanosleep, const struct timespec *, req,
		struct timespec *, rem)
DEFINE_SYSCALL0(24, i32, sched_yield)
DEFINE_SYSCALL2(96, i32, gettimeofday, struct timeval *, tv, void *, tz)
DEFINE_SYSCALL2(164, i32, settimeofday, const struct timeval *, tv,
		const void *, tz)
DEFINE_SYSCALL1(291, i32, epoll_create1, i32, flags)
DEFINE_SYSCALL6(281, i32, epoll_pwait, i32, epfd, struct epoll_event *, events,
		i32, maxevents, i32, timeout, const sigset_t *, sigs, i32, size)
DEFINE_SYSCALL4(233, i32, epoll_ctl, i32, epfd, i32, op, i32, fd,
		struct epoll_event *, event)
DEFINE_SYSCALL4(257, i32, openat, i32, dfd, const u8 *, pathname, i32, flags,
		u32, mode)
DEFINE_SYSCALL3(8, i64, lseek, i32, fd, i64, offset, i32, whence)
DEFINE_SYSCALL1(75, i32, fdatasync, i32, fd)
DEFINE_SYSCALL2(77, i32, ftruncate, i32, fd, i64, length)
DEFINE_SYSCALL3(38, i32, setitimer, i32, which, const struct itimerval *,
		new_value, struct itimerval *, old_value)
DEFINE_SYSCALL2(435, i32, clone3, struct clone_args *, args, u64, size)
DEFINE_SYSCALL6(202, i64, futex, u32 *, uaddr, i32, futex_op, u32, val,
		const struct timespec *, timeout, u32 *, uaddr2, u32, val3)
DEFINE_SYSCALL4(13, i32, rt_sigaction, i32, signum, const struct rt_sigaction *,
		act, struct rt_sigaction *, oldact, u64, sigsetsize)
DEFINE_SYSCALL0(39, i32, getpid)
DEFINE_SYSCALL2(62, i32, kill, i32, pid, i32, signal)
#endif /* Arch */

i32 clone3(struct clone_args *args, u64 size) {
#if TEST == 1
	if (_debug_fail_clone3) return -1;
#endif /* TEST */
	i32 ret = syscall_clone3(args, size);
	SET_ERR
}

i32 pipe2(i32 fds[2], i32 flags) {
	i32 ret;
	if (_debug_fail_pipe2) return -1;
	ret = syscall_pipe2(fds, flags);
	SET_ERR
}
i32 unlinkat(i32 dfd, const u8 *path, i32 flags) {
	i32 ret = syscall_unlinkat(dfd, path, flags);
	SET_ERR
}

i64 write(i32 fd, const void *buf, u64 count) {
	i64 ret = 0;
	if ((fd == 1 || fd == 2) && _debug_no_write) return count;
	ret = syscall_write(fd, buf, count);
	SET_ERR
}
i64 read(i32 fd, void *buf, u64 count) {
	i64 ret = syscall_read(fd, buf, count);
	SET_ERR
}

i32 sched_yield(void) {
	i32 ret = syscall_sched_yield();
	SET_ERR
}

#ifdef COVERAGE
void __gcov_dump(void);
#endif /* COVERAGE */

void exit(i32 status) {
	execute_exits();
#ifdef COVERAGE
	SYSCALL_EXIT_COV
#else
	SYSCALL_EXIT
#endif /* COVERAGE */
}
i32 munmap(void *addr, u64 len) {
	i32 ret = syscall_munmap(addr, len);
	SET_ERR
}

i32 close(i32 fd) {
	i32 ret = syscall_close(fd);
	SET_ERR
}

i32 fcntl(i32 fd, i32 op, ...) {
	__builtin_va_list ap;
	i64 arg;
	i32 ret;

#if TEST == 1
	if (_debug_fail_fcntl) return -1;
#endif /* TEST */

	__builtin_va_start(ap, op);

	switch (op) {
		case F_DUPFD:
		case F_SETFD:
		case F_SETFL:
		case F_SETOWN:
		case F_SETLEASE:
			arg = __builtin_va_arg(ap, i64);
			break;
		case F_GETFD:
		case F_GETFL:
		case F_GETOWN:
		case F_GETLEASE:
			arg = 0;
			break;
		default:
			arg = 0;
			break;
	}

	__builtin_va_end(ap);

	ret = syscall_fcntl(fd, op, arg);

	SET_ERR
}

i32 fdatasync(i32 fd) {
	i32 ret = syscall_fdatasync(fd);
	SET_ERR
}

i32 ftruncate(i32 fd, i64 length) {
	i32 ret = syscall_ftruncate(fd, length);
	SET_ERR
}

i32 connect(i32 sockfd, const struct sockaddr *addr, u32 addrlen) {
	i32 ret = syscall_connect(sockfd, addr, addrlen);
	SET_ERR
}

i32 setsockopt(i32 sockfd, i32 level, i32 optname, const void *optval,
	       u32 optlen) {
#if TEST == 1
	if (_debug_fail_setsockopt) return -1;
#endif /* TEST */

	i32 ret = syscall_setsockopt(sockfd, level, optname, optval, optlen);
	SET_ERR
}

i32 getsockopt(i32 sockfd, i32 level, i32 optname, void *optval, u32 *optlen) {
#if TEST == 1
	if (_debug_fail_setsockopt) return -1;
#endif /* TEST */

	i32 ret = syscall_getsockopt(sockfd, level, optname, optval, optlen);
	SET_ERR
}

i32 bind(i32 sockfd, const struct sockaddr *addr, u32 addrlen) {
	i32 ret = syscall_bind(sockfd, addr, addrlen);
	SET_ERR
}
i32 listen(i32 sockfd, i32 backlog) {
#if TEST == 1
	if (_debug_fail_listen) return -1;
#endif /* TEST */

	i32 ret = syscall_listen(sockfd, backlog);
	SET_ERR
}
i32 getsockname(i32 sockfd, struct sockaddr *addr, u32 *addrlen) {
#if TEST == 1
	if (_debug_fail_getsockbyname) return -1;
#endif /* TEST */

	i32 ret = syscall_getsockname(sockfd, addr, addrlen);
	SET_ERR
}
i32 accept(i32 sockfd, struct sockaddr *addr, u32 *addrlen) {
	i32 ret = syscall_accept(sockfd, addr, addrlen);
	SET_ERR
}
i32 shutdown(i32 sockfd, i32 how) {
	i32 ret = syscall_shutdown(sockfd, how);
	SET_ERR
}

i32 socket(i32 domain, i32 type, i32 protocol) {
	i32 ret = syscall_socket(domain, type, protocol);
	SET_ERR
}

i64 futex(u32 *uaddr, i32 futex_op, u32 val, const struct timespec *timeout,
	  u32 *uaddr2, u32 val3) {
	i64 ret = syscall_futex(uaddr, futex_op, val, timeout, uaddr2, val3);
	SET_ERR
}
i32 getrandom(void *buf, u64 len, u32 flags) {
	u64 total;
	if (len > 256) {
		err = EIO;
		return -1;
	}
	if (!buf) {
		err = EFAULT;
		return -1;
	}

	total = 0;
	while (total < len) {
		i64 ret = syscall_getrandom(buf, len, flags);
		SET_ERR_VALUE
		total += ret;
	}
	return 0;
}

void *mmap(void *addr, u64 length, i32 prot, i32 flags, i32 fd, i64 offset) {
	void *ret;
	ret = syscall_mmap(addr, length, prot, flags, fd, offset);
	SET_ERR_I64_VOID_PTR
}

i32 nanosleep(const struct timespec *req, struct timespec *rem) {
	i32 ret = syscall_nanosleep(req, rem);
	SET_ERR
}
i32 gettimeofday(struct timeval *tv, void *tz) {
	i32 ret = syscall_gettimeofday(tv, tz);
	SET_ERR
}

i32 settimeofday(const struct timeval *tv, const struct timezone *tz) {
	i32 ret = syscall_settimeofday(tv, tz);
	SET_ERR
}
i32 epoll_create1(i32 flags) {
#if TEST == 1
	if (_debug_fail_epoll_create1) return -1;
#endif /* TEST */

	i32 ret = syscall_epoll_create1(flags);
	SET_ERR
}

i32 epoll_pwait(i32 epfd, struct epoll_event *events, i32 maxevents,
		i32 timeout, const sigset_t *sigmask, u64 size) {
	i32 ret = syscall_epoll_pwait(epfd, events, maxevents, timeout, sigmask,
				      size);
	SET_ERR
}

i32 epoll_ctl(i32 epfd, i32 op, i32 fd, struct epoll_event *event) {
	i32 ret = syscall_epoll_ctl(epfd, op, fd, event);
	SET_ERR
}

i32 openat(i32 dfd, const u8 *pathname, i32 flags, u32 mode) {
	i32 ret = syscall_openat(dfd, pathname, flags, mode);
	SET_ERR
}

i64 lseek(i32 fd, i64 offset, i32 whence) {
	i64 ret = syscall_lseek(fd, offset, whence);
	SET_ERR
}

i32 setitimer(i32 which, const struct itimerval *new_value,
	      struct itimerval *old_value) {
	i32 ret = syscall_setitimer(which, new_value, old_value);
	SET_ERR
}

i32 rt_sigaction(i32 signum, const struct rt_sigaction *act,
		 struct rt_sigaction *oldact, u64 sigsetsize) {
	i32 ret = syscall_rt_sigaction(signum, act, oldact, sigsetsize);
	SET_ERR
}

void restorer(void){SYSCALL_RESTORER}

i32 waitid(i32 i32ype, i32 id, siginfo_t *sigs, i32 options) {
	i32 ret = syscall_waitid(i32ype, id, sigs, options);
	SET_ERR
}

i32 getpid(void) {
	i32 ret = syscall_getpid();
	SET_ERR
}
i32 kill(i32 pid, i32 signal) {
	i32 ret = syscall_kill(pid, signal);
	SET_ERR
}
