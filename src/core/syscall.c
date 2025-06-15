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

#define SET_ERR             \
	if (ret < 0) {      \
		err = -ret; \
		return -1;  \
	}                   \
	return ret;

#ifdef __aarch64__
#define DEFINE_SYSCALL0(sysno, ret_type, name) \
	static ret_type syscall_##name(void) { \
		long result;                   \
		__asm__ volatile(              \
		    "mov x8, %1\n"             \
		    "svc #0\n"                 \
		    "mov %0, x0\n"             \
		    : "=r"(result)             \
		    : "r"((long)(sysno))       \
		    : "x8", "x0", "memory");   \
		return (ret_type)result;       \
	}

#define DEFINE_SYSCALL1(sysno, ret_type, name, type1, arg1) \
	static ret_type syscall_##name(type1 arg1) {        \
		long result;                                \
		__asm__ volatile(                           \
		    "mov x8, %1\n"                          \
		    "mov x0, %2\n"                          \
		    "svc #0\n"                              \
		    "mov %0, x0\n"                          \
		    : "=r"(result)                          \
		    : "r"((long)(sysno)), "r"((long)(arg1)) \
		    : "x8", "x0", "memory");                \
		return (ret_type)result;                    \
	}

#define DEFINE_SYSCALL2(sysno, ret_type, name, type1, arg1, type2, arg2)       \
	static ret_type syscall_##name(type1 arg1, type2 arg2) {               \
		long result;                                                   \
		__asm__ volatile(                                              \
		    "mov x8, %1\n"                                             \
		    "mov x0, %2\n"                                             \
		    "mov x1, %3\n"                                             \
		    "svc #0\n"                                                 \
		    "mov %0, x0\n"                                             \
		    : "=r"(result)                                             \
		    : "r"((long)(sysno)), "r"((long)(arg1)), "r"((long)(arg2)) \
		    : "x8", "x0", "x1", "memory");                             \
		return (ret_type)result;                                       \
	}

#define DEFINE_SYSCALL3(sysno, ret_type, name, type1, arg1, type2, arg2,     \
			type3, arg3)                                         \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3) { \
		long result;                                                 \
		__asm__ volatile(                                            \
		    "mov x8, %1\n"                                           \
		    "mov x0, %2\n"                                           \
		    "mov x1, %3\n"                                           \
		    "mov x2, %4\n"                                           \
		    "svc #0\n"                                               \
		    "mov %0, x0\n"                                           \
		    : "=r"(result)                                           \
		    : "r"((long)(sysno)), "r"((long)(arg1)),                 \
		      "r"((long)(arg2)), "r"((long)(arg3))                   \
		    : "x8", "x0", "x1", "x2", "memory");                     \
		return (ret_type)result;                                     \
	}

#define DEFINE_SYSCALL4(sysno, ret_type, name, type1, arg1, type2, arg2,      \
			type3, arg3, type4, arg4)                             \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3,    \
				       type4 arg4) {                          \
		long result;                                                  \
		__asm__ volatile(                                             \
		    "mov x8, %1\n"                                            \
		    "mov x0, %2\n"                                            \
		    "mov x1, %3\n"                                            \
		    "mov x2, %4\n"                                            \
		    "mov x3, %5\n"                                            \
		    "svc #0\n"                                                \
		    "mov %0, x0\n"                                            \
		    : "=r"(result)                                            \
		    : "r"((long)(sysno)), "r"((long)(arg1)),                  \
		      "r"((long)(arg2)), "r"((long)(arg3)), "r"((long)(arg4)) \
		    : "x8", "x0", "x1", "x2", "x3", "memory");                \
		return (ret_type)result;                                      \
	}

#define DEFINE_SYSCALL5(sysno, ret_type, name, type1, arg1, type2, arg2,       \
			type3, arg3, type4, arg4, type5, arg5)                 \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3,     \
				       type4 arg4, type5 arg5) {               \
		long result;                                                   \
		__asm__ volatile(                                              \
		    "mov x8, %1\n"                                             \
		    "mov x0, %2\n"                                             \
		    "mov x1, %3\n"                                             \
		    "mov x2, %4\n"                                             \
		    "mov x3, %5\n"                                             \
		    "mov x4, %6\n"                                             \
		    "svc #0\n"                                                 \
		    "mov %0, x0\n"                                             \
		    : "=r"(result)                                             \
		    : "r"((long)(sysno)), "r"((long)(arg1)),                   \
		      "r"((long)(arg2)), "r"((long)(arg3)), "r"((long)(arg4)), \
		      "r"((long)(arg5))                                        \
		    : "x8", "x0", "x1", "x2", "x3", "x4", "memory");           \
		return (ret_type)result;                                       \
	}

#define DEFINE_SYSCALL6(sysno, ret_type, name, type1, arg1, type2, arg2,       \
			type3, arg3, type4, arg4, type5, arg5, type6, arg6)    \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3,     \
				       type4 arg4, type5 arg5, type6 arg6) {   \
		long result;                                                   \
		__asm__ volatile(                                              \
		    "mov x8, %1\n"                                             \
		    "mov x0, %2\n"                                             \
		    "mov x1, %3\n"                                             \
		    "mov x2, %4\n"                                             \
		    "mov x3, %5\n"                                             \
		    "mov x4, %6\n"                                             \
		    "mov x5, %7\n"                                             \
		    "svc #0\n"                                                 \
		    "mov %0, x0\n"                                             \
		    : "=r"(result)                                             \
		    : "r"((long)(sysno)), "r"((long)(arg1)),                   \
		      "r"((long)(arg2)), "r"((long)(arg3)), "r"((long)(arg4)), \
		      "r"((long)(arg5)), "r"((long)(arg6))                     \
		    : "x8", "x0", "x1", "x2", "x3", "x4", "x5", "memory");     \
		return (ret_type)result;                                       \
	}

static void syscall_exit(int status) {
	__asm__ volatile(
	    "mov x8, #93\n" /* exit syscall number (ARM64) */
	    "mov x0, %0\n"  /* status */
	    "svc #0\n"
	    :
	    : "r"((long)status)
	    : "x8", "x0", "memory");
}

static void syscall_restorer(void) {
	__asm__ volatile(
	    "mov x8, #139\n"
	    "svc #0\n" ::
		: "x8", "memory");
}

static int syscall_waitid(int idtype, int id, siginfo_t *infop, int options) {
	long result;
	__asm__ volatile(
	    "mov x8, #95\n"
	    "mov x0, %1\n"
	    "mov x1, %2\n"
	    "mov x2, %3\n"
	    "mov x3, %4\n"
	    "svc #0\n"
	    "mov %0, x0\n"
	    : "=r"(result)
	    : "r"((long)idtype), "r"((long)id), "r"((long)infop),
	      "r"((long)options)
	    : "x8", "x0", "x1", "x2", "x3", "memory");
	return (int)result;
}

#elif defined(__amd64__)

#define DEFINE_SYSCALL0(sysno, ret_type, name)                        \
	static ret_type syscall_##name(void) {                        \
		long result;                                          \
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
		long result;                                                  \
		__asm__ volatile("movq $" #sysno                              \
				 ", %%rax\n"                                  \
				 "movq %1, %%rdi\n"                           \
				 "syscall\n"                                  \
				 "movq %%rax, %0\n"                           \
				 : "=r"(result)                               \
				 : "r"((long)(arg1))                          \
				 : "%rax", "%rcx", "%r11", "%rdi", "memory"); \
		return (ret_type)result;                                      \
	}

#define DEFINE_SYSCALL2(sysno, ret_type, name, type1, arg1, type2, arg2)   \
	static ret_type syscall_##name(type1 arg1, type2 arg2) {           \
		long result;                                               \
		__asm__ volatile("movq $" #sysno                           \
				 ", %%rax\n"                               \
				 "movq %1, %%rdi\n"                        \
				 "movq %2, %%rsi\n"                        \
				 "syscall\n"                               \
				 "movq %%rax, %0\n"                        \
				 : "=r"(result)                            \
				 : "r"((long)(arg1)), "r"((long)(arg2))    \
				 : "%rax", "%rcx", "%r11", "%rdi", "%rsi", \
				   "memory");                              \
		return (ret_type)result;                                   \
	}

#define DEFINE_SYSCALL3(sysno, ret_type, name, type1, arg1, type2, arg2,      \
			type3, arg3)                                          \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3) {  \
		long result;                                                  \
		__asm__ volatile(                                             \
		    "movq $" #sysno                                           \
		    ", %%rax\n"                                               \
		    "movq %1, %%rdi\n"                                        \
		    "movq %2, %%rsi\n"                                        \
		    "movq %3, %%rdx\n"                                        \
		    "syscall\n"                                               \
		    "movq %%rax, %0\n"                                        \
		    : "=r"(result)                                            \
		    : "r"((long)(arg1)), "r"((long)(arg2)), "r"((long)(arg3)) \
		    : "%rax", "%rdi", "%rsi", "%rdx", "%rcx", "%r11",         \
		      "memory");                                              \
		return (ret_type)result;                                      \
	}

#define DEFINE_SYSCALL4(sysno, ret_type, name, type1, arg1, type2, arg2,   \
			type3, arg3, type4, arg4)                          \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3, \
				       type4 arg4) {                       \
		long result;                                               \
		__asm__ volatile("movq $" #sysno                           \
				 ", %%rax\n"                               \
				 "movq %1, %%rdi\n"                        \
				 "movq %2, %%rsi\n"                        \
				 "movq %3, %%rdx\n"                        \
				 "movq %4, %%r10\n"                        \
				 "syscall\n"                               \
				 "movq %%rax, %0\n"                        \
				 : "=r"(result)                            \
				 : "r"((long)(arg1)), "r"((long)(arg2)),   \
				   "r"((long)(arg3)), "r"((long)(arg4))    \
				 : "%rax", "%rcx", "%r11", "%rdi", "%rsi", \
				   "%rdx", "%r10", "memory");              \
		return (ret_type)result;                                   \
	}

#define DEFINE_SYSCALL5(sysno, ret_type, name, type1, arg1, type2, arg2,   \
			type3, arg3, type4, arg4, type5, arg5)             \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3, \
				       type4 arg4, type5 arg5) {           \
		long result;                                               \
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
				 : "r"((long)(arg1)), "r"((long)(arg2)),   \
				   "r"((long)(arg3)), "r"((long)(arg4)),   \
				   "r"((long)(arg5))                       \
				 : "%rax", "%rcx", "%r11", "%rdi", "%rsi", \
				   "%rdx", "%r10", "%r8", "memory");       \
		return (ret_type)result;                                   \
	}

#define DEFINE_SYSCALL6(sysno, ret_type, name, type1, arg1, type2, arg2,     \
			type3, arg3, type4, arg4, type5, arg5, type6, arg6)  \
	static ret_type syscall_##name(type1 arg1, type2 arg2, type3 arg3,   \
				       type4 arg4, type5 arg5, type6 arg6) { \
		long result;                                                 \
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
				 : "r"((long)(arg1)), "r"((long)(arg2)),     \
				   "r"((long)(arg3)), "r"((long)(arg4)),     \
				   "r"((long)(arg5)), "r"((long)(arg6))      \
				 : "%rax", "%rcx", "%r11", "%rdi", "%rsi",   \
				   "%rdx", "%r10", "%r8", "%r9", "memory");  \
		return (ret_type)result;                                     \
	}

static void syscall_exit(int status) {
	__asm__ volatile(
	    "movq $60, %%rax\n" /* exit syscall number (Linux) */
	    "movq %0, %%rdi\n"	/* status */
	    "syscall\n"
	    :
	    : "r"((long)status)			       /* Input */
	    : "%rax", "%rdi", "%rcx", "%r11", "memory" /* Clobbered */
	);
}

static void syscall_restorer(void) {
	__asm__ volatile(
	    "movq $15, %%rax\n" /* rt_sigreturn (x86-64) */
	    "syscall\n"
	    :
	    :
	    : "%rax", "%rcx", "%r11", "memory");
}

static int syscall_waitid(int idtype, int32_t id, siginfo_t *infop, int options) {
	long result;
	__asm__ volatile(
	    "movq $247, %%rax\n"
	    "movq %1, %%rdi\n"
	    "movq %2, %%rsi\n"
	    "movq %3, %%rdx\n"
	    "movq %4, %%r10\n"
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)
	    : "r"((long)idtype), "r"((long)id), "r"((long)infop),
	      "r"((long)options)
	    : "rax", "rdi", "rsi", "rdx", "r10", "rcx", "r11", "memory");
	return (int)result;
}

#endif /* Arch */

#ifdef __aarch64__
DEFINE_SYSCALL2(59, int, pipe2, int *, fds, int, flags)
DEFINE_SYSCALL3(35, int, unlinkat, int, dfd, const char *, path, int, flag)
DEFINE_SYSCALL3(64, int64_t, write, int, fd, const void *, buf, uint64_t, count)
DEFINE_SYSCALL3(63, int64_t, read, int, fd, void *, buf, uint64_t, count)
DEFINE_SYSCALL2(215, int, munmap, void *, addr, uint64_t, len)
DEFINE_SYSCALL1(57, int, close, int, fd)
DEFINE_SYSCALL3(25, int, fcntl, int, fd, int, cmd, long, arg)
DEFINE_SYSCALL3(203, int, connect, int, sockfd, const struct sockaddr *, addr,
		uint32_t, addrlen)
DEFINE_SYSCALL5(208, int, setsockopt, int, sockfd, int, level, int, optname,
		const void *, optval, uint32_t, optlen)
DEFINE_SYSCALL3(200, int, bind, int, sockfd, const struct sockaddr *, addr,
		uint32_t, addrlen)
DEFINE_SYSCALL2(201, int, listen, int, sockfd, int, backlog)
DEFINE_SYSCALL3(204, int, getsockname, int, sockfd, struct sockaddr *, addr,
		uint32_t *, addrlen)
DEFINE_SYSCALL3(202, int, accept, int, sockfd, struct sockaddr *, addr,
		uint32_t *, addrlen)
DEFINE_SYSCALL2(210, int, shutdown, int, sockfd, int, how)
DEFINE_SYSCALL3(198, int, socket, int, domain, int, type, int, protocol)
DEFINE_SYSCALL3(278, int, getrandom, void *, buffer, uint64_t, length,
		uint32_t, flags)
DEFINE_SYSCALL6(222, void *, mmap, void *, addr, uint64_t, length, int, prot, int,
		flags, int, fd, long, offset)
DEFINE_SYSCALL2(101, int, nanosleep, const struct timespec *, req,
		struct timespec *, rem)
DEFINE_SYSCALL0(124, int, sched_yield)
DEFINE_SYSCALL2(169, int, gettimeofday, struct timeval *, tv, void *, tz)
DEFINE_SYSCALL2(170, int, settimeofday, const struct timeval *, tv,
		const void *, tz)
DEFINE_SYSCALL1(20, int, epoll_create1, int, flags)
DEFINE_SYSCALL6(22, int, epoll_pwait, int, epfd, struct epoll_event *, events,
		int, maxevents, int, timeout, const sigset_t *, sigs, int, size)
DEFINE_SYSCALL4(21, int, epoll_ctl, int, epfd, int, op, int, fd,
		struct epoll_event *, event)
DEFINE_SYSCALL4(56, int, openat, int, dfd, const char *, pathname, int, flags,
		uint32_t, mode)
DEFINE_SYSCALL3(62, int64_t, lseek, int, fd, int64_t, offset, int, whence)
DEFINE_SYSCALL1(83, int, fdatasync, int, fd)
DEFINE_SYSCALL2(46, int, ftruncate, int, fd, int64_t, length)
DEFINE_SYSCALL3(103, int, setitimer, int32_t, which,
		const struct itimerval *, new_value, struct itimerval *,
		old_value)
DEFINE_SYSCALL2(435, int, clone3, struct clone_args *, args, uint64_t, size)
DEFINE_SYSCALL6(98, long, futex, uint32_t *, uaddr, int, futex_op, uint32_t,
		val, const struct timespec *, timeout, uint32_t *, uaddr2,
		uint32_t, val3)
DEFINE_SYSCALL4(134, int, rt_sigaction, int, signum,
		const struct rt_sigaction *, act, struct rt_sigaction *, oldact,
		uint64_t, sigsetsize)
DEFINE_SYSCALL0(172, int32_t, getpid)
DEFINE_SYSCALL2(129, int, kill, int32_t, pid, int, signal)
#elif defined(__amd64__)
/* System call definitions */
DEFINE_SYSCALL2(293, int, pipe2, int *, fds, int, flags)
DEFINE_SYSCALL3(263, int, unlinkat, int, dfd, const char *, path, int, flags)
DEFINE_SYSCALL3(1, int64_t, write, int, fd, const void *, buf, uint64_t, count)
DEFINE_SYSCALL3(0, int64_t, read, int, fd, void *, buf, uint64_t, count)
DEFINE_SYSCALL2(11, int, munmap, void *, addr, uint64_t, len)
DEFINE_SYSCALL1(3, int, close, int, fd)
DEFINE_SYSCALL3(72, int, fcntl, int, fd, int, cmd, long, arg)
DEFINE_SYSCALL3(42, int, connect, int, sockfd, const struct sockaddr *, addr,
		uint32_t, addrlen)
DEFINE_SYSCALL5(54, int, setsockopt, int, sockfd, int, level, int, optname,
		const void *, optval, uint32_t, optlen)
DEFINE_SYSCALL3(49, int, bind, int, sockfd, const struct sockaddr *, addr,
		uint32_t, addrlen)
DEFINE_SYSCALL2(50, int, listen, int, sockfd, int, backlog)
DEFINE_SYSCALL3(51, int, getsockname, int, sockfd, struct sockaddr *, addr,
		uint32_t *, addrlen)
DEFINE_SYSCALL3(43, int, accept, int, sockfd, struct sockaddr *, addr,
		uint32_t *, addrlen)
DEFINE_SYSCALL2(48, int, shutdown, int, sockfd, int, how)
DEFINE_SYSCALL3(41, int, socket, int, domain, int, type, int, protocol)
DEFINE_SYSCALL3(318, int, getrandom, void *, buffer, uint64_t, length,
		uint32_t, flags)
DEFINE_SYSCALL6(9, void *, mmap, void *, addr, uint64_t, length, int, prot, int,
		flags, int, fd, long, offset)
DEFINE_SYSCALL2(35, int, nanosleep, const struct timespec *, req,
		struct timespec *, rem)
DEFINE_SYSCALL0(24, int, sched_yield)
DEFINE_SYSCALL2(96, int, gettimeofday, struct timeval *, tv, void *, tz)
DEFINE_SYSCALL2(164, int, settimeofday, const struct timeval *, tv,
		const void *, tz)
DEFINE_SYSCALL1(291, int, epoll_create1, int, flags)
DEFINE_SYSCALL6(281, int, epoll_pwait, int, epfd, struct epoll_event *, events,
		int, maxevents, int, timeout, const sigset_t *, sigs, int, size)
DEFINE_SYSCALL4(233, int, epoll_ctl, int, epfd, int, op, int, fd,
		struct epoll_event *, event)
DEFINE_SYSCALL4(257, int, openat, int, dfd, const char *, pathname, int, flags,
		uint32_t, mode)
DEFINE_SYSCALL3(8, int64_t, lseek, int, fd, int64_t, offset, int, whence)
DEFINE_SYSCALL1(75, int, fdatasync, int, fd)
DEFINE_SYSCALL2(77, int, ftruncate, int, fd, int64_t, length)
DEFINE_SYSCALL3(38, int, setitimer, int32_t, which,
		const struct itimerval *, new_value, struct itimerval *,
		old_value)
DEFINE_SYSCALL2(435, int, clone3, struct clone_args *, args, uint64_t, size)
DEFINE_SYSCALL6(202, long, futex, uint32_t *, uaddr, int, futex_op, uint32_t,
		val, const struct timespec *, timeout, uint32_t *, uaddr2,
		uint32_t, val3)
DEFINE_SYSCALL4(13, int, rt_sigaction, int, signum, const struct rt_sigaction *,
		act, struct rt_sigaction *, oldact, uint64_t, sigsetsize)
DEFINE_SYSCALL0(39, int32_t, getpid)
DEFINE_SYSCALL2(62, int, kill, int32_t, pid, int, signal)
#endif /* Arch */

int clone3(struct clone_args *args, uint64_t size) {
	int ret = syscall_clone3(args, size);
	SET_ERR
}

int pipe2(int fds[2], int flags) {
	int ret = syscall_pipe2(fds, flags);
	SET_ERR
}
int unlinkat(int dfd, const char *path, int flags) {
	int ret = syscall_unlinkat(dfd, path, flags);
	SET_ERR
}
int64_t write(int fd, const void *buf, uint64_t count) {
	int64_t ret = syscall_write(fd, buf, count);
	SET_ERR
}
int64_t read(int fd, void *buf, uint64_t count) {
	int64_t ret = syscall_read(fd, buf, count);
	SET_ERR
}

int sched_yield(void) {
	int ret = syscall_sched_yield();
	SET_ERR
}

#ifdef COVERAGE
void __gcov_dump(void);
#endif /* COVERAGE */

void exit(int status) {
	execute_exits();
#ifdef COVERAGE
	__gcov_dump();
#endif
	syscall_exit(status);
	while (true);
}
int munmap(void *addr, uint64_t len) {
	int ret = syscall_munmap(addr, len);
	SET_ERR
}

int close(int fd) {
	int ret = syscall_close(fd);
	SET_ERR
}

int fcntl(int fd, int op, ...) {
	__builtin_va_list ap;
	long arg;
	int ret;

	__builtin_va_start(ap, op);

	switch (op) {
		case F_DUPFD:
		case F_SETFD:
		case F_SETFL:
		case F_SETOWN:
		case F_SETLEASE:
			arg = __builtin_va_arg(ap, long);
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

int fdatasync(int fd) {
	int ret = syscall_fdatasync(fd);
	SET_ERR
}

int ftruncate(int fd, int64_t length) {
	int ret = syscall_ftruncate(fd, length);
	SET_ERR
}

int connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen) {
	int ret = syscall_connect(sockfd, addr, addrlen);
	SET_ERR
}

int setsockopt(int sockfd, int level, int optname, const void *optval,
	       uint32_t optlen) {
	int ret = syscall_setsockopt(sockfd, level, optname, optval, optlen);
	SET_ERR
}

int bind(int sockfd, const struct sockaddr *addr, uint32_t addrlen) {
	int ret = syscall_bind(sockfd, addr, addrlen);
	SET_ERR
}
int listen(int sockfd, int backlog) {
	int ret = syscall_listen(sockfd, backlog);
	SET_ERR
}
int getsockname(int sockfd, struct sockaddr *addr, uint32_t *addrlen) {
	int ret = syscall_getsockname(sockfd, addr, addrlen);
	SET_ERR
}
int accept(int sockfd, struct sockaddr *addr, uint32_t *addrlen) {
	int ret = syscall_accept(sockfd, addr, addrlen);
	SET_ERR
}
int shutdown(int sockfd, int how) {
	int ret = syscall_shutdown(sockfd, how);
	SET_ERR
}

int socket(int domain, int type, int protocol) {
	int ret = syscall_socket(domain, type, protocol);
	SET_ERR
}

long futex(uint32_t *uaddr, int futex_op, uint32_t val,
	   const struct timespec *timeout, uint32_t *uaddr2, uint32_t val3) {
	long ret = syscall_futex(uaddr, futex_op, val, timeout, uaddr2, val3);
	SET_ERR
}
int getrandom(void *buf, uint64_t len, uint32_t flags) {
	uint64_t total;
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
		int64_t ret = syscall_getrandom(buf, len, flags);

		if (ret < 0) {
			err = -ret;
			return -1;
		}
		total += ret;
	}
	return 0;
}

void *mmap(void *addr, uint64_t length, int prot, int flags, int fd,
	   int64_t offset) {
	void *ret;
	ret = syscall_mmap(addr, length, prot, flags, fd, offset);

	if ((long)ret < 0) {
		err = -(long)ret;
		return (void *)-1;
	}
	return ret;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
	int ret = syscall_nanosleep(req, rem);
	SET_ERR
}
int gettimeofday(struct timeval *tv, void *tz) {
	int ret = syscall_gettimeofday(tv, tz);
	SET_ERR
}

int settimeofday(const struct timeval *tv, const struct timezone *tz) {
	int ret = syscall_settimeofday(tv, tz);
	SET_ERR
}
int epoll_create1(int flags) {
	int ret = syscall_epoll_create1(flags);
	SET_ERR
}

int epoll_pwait(int epfd, struct epoll_event *events, int maxevents,
		int timeout, const sigset_t *sigmask, uint64_t size) {
	int ret = syscall_epoll_pwait(epfd, events, maxevents, timeout, sigmask,
				      size);
	SET_ERR
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
	int ret = syscall_epoll_ctl(epfd, op, fd, event);
	SET_ERR
}

int openat(int dfd, const char *pathname, int flags, uint32_t mode) {
	int ret = syscall_openat(dfd, pathname, flags, mode);
	SET_ERR
}

int64_t lseek(int fd, int64_t offset, int whence) {
	int64_t ret = syscall_lseek(fd, offset, whence);
	SET_ERR
}

int setitimer(int32_t which, const struct itimerval *new_value,
	      struct itimerval *old_value) {
	int ret = syscall_setitimer(which, new_value, old_value);
	SET_ERR
}

int rt_sigaction(int signum, const struct rt_sigaction *act,
		 struct rt_sigaction *oldact, uint64_t sigsetsize) {
	int ret = syscall_rt_sigaction(signum, act, oldact, sigsetsize);
	SET_ERR
}

void restorer(void) { syscall_restorer(); }

int waitid(int32_t int32_type, int32_t id, siginfo_t *sigs, int options) {
	int32_t ret = syscall_waitid(int32_type, id, sigs, options);
	SET_ERR
}

int32_t getpid(void) {
	int32_t ret = syscall_getpid();
	SET_ERR
}
int kill(int32_t pid, int signal) {
	int ret = syscall_kill(pid, signal);
	SET_ERR
}
