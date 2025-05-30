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

#ifdef __linux__
#define _GNU_SOURCE
#endif /* __linux__ */

#define STATIC_ASSERT(condition, message) \
	typedef char static_assert_##message[(condition) ? 1 : -1]

#include <fcntl.h>
#include <sys.h>
#ifdef __linux__
#include <sys/epoll.h>
STATIC_ASSERT(sizeof(Event) == sizeof(struct epoll_event), sizes_match);
#elif defined(__APPLE__)
#include <sys/event.h>
STATIC_ASSERT(sizeof(Event) == sizeof(struct kevent), sizes_match);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

#ifdef __linux__
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

/* System call definitions */
DEFINE_SYSCALL0(57, pid_t, fork)
DEFINE_SYSCALL1(22, int, pipe, int *, fds)
DEFINE_SYSCALL1(87, int, unlink, const char *, path)
DEFINE_SYSCALL3(1, ssize_t, write, int, fd, const void *, buf, size_t, count)
DEFINE_SYSCALL3(0, ssize_t, read, int, fd, void *, buf, size_t, count)
DEFINE_SYSCALL2(11, int, munmap, void *, addr, size_t, len)
DEFINE_SYSCALL1(3, int, close, int, fd)
DEFINE_SYSCALL3(72, int, fcntl, int, fd, int, cmd, long, arg)
DEFINE_SYSCALL3(42, int, connect, int, sockfd, const struct sockaddr *, addr,
		unsigned int, addrlen)
DEFINE_SYSCALL5(54, int, setsockopt, int, sockfd, int, level, int, optname,
		const void *, optval, unsigned int, optlen)
DEFINE_SYSCALL3(49, int, bind, int, sockfd, const struct sockaddr *, addr,
		unsigned int, addrlen)
DEFINE_SYSCALL2(50, int, listen, int, sockfd, int, backlog)
DEFINE_SYSCALL3(51, int, getsockname, int, sockfd, struct sockaddr *, addr,
		unsigned int *, addrlen)
DEFINE_SYSCALL3(43, int, accept, int, sockfd, struct sockaddr *, addr,
		unsigned int *, addrlen)
DEFINE_SYSCALL2(48, int, shutdown, int, sockfd, int, how)
DEFINE_SYSCALL3(41, int, socket, int, domain, int, type, int, protocol)
DEFINE_SYSCALL2(318, int, getentropy, void *, buffer, size_t, length)
DEFINE_SYSCALL6(9, void *, mmap, void *, addr, size_t, length, int, prot, int,
		flags, int, fd, long, offset)
DEFINE_SYSCALL0(24, int, sched_yield)
DEFINE_SYSCALL2(35, int, nanosleep, const struct timespec *, req,
		struct timespec *, rem)
DEFINE_SYSCALL2(96, int, gettimeofday, struct timeval *, tv, void *, tz)
DEFINE_SYSCALL2(164, int, settimeofday, const struct timeval *, tv,
		const void *, tz)
DEFINE_SYSCALL1(291, int, epoll_create1, int, flags)
DEFINE_SYSCALL4(232, int, epoll_wait, int, epfd, struct epoll_event *, events,
		int, maxevents, int, timeout)
DEFINE_SYSCALL4(233, int, epoll_ctl, int, epfd, int, op, int, fd,
		struct epoll_event *, event)
DEFINE_SYSCALL3(2, int, open, const char *, pathname, int, flags, mode_t, mode)
DEFINE_SYSCALL3(8, off_t, lseek, int, fd, off_t, offset, int, whence)

pid_t fork(void) {
	pid_t ret = syscall_fork();
	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}
int pipe(int fds[2]) { return syscall_pipe(fds); }
int unlink(const char *path) { return syscall_unlink(path); }
ssize_t write(int fd, const void *buf, size_t count) {
	return syscall_write(fd, buf, count);
}
ssize_t read(int fd, void *buf, size_t count) {
	return syscall_read(fd, buf, count);
}
void exit(int status) {
	syscall_exit(status);
	while (true);
}
int munmap(void *addr, size_t len) { return syscall_munmap(addr, len); }

int close(int fd) { return syscall_close(fd); }

int fcntl(int fd, int op, ...) {
	__builtin_va_list ap;
	long arg;
	int ret;

	/* Initialize the variable argument list */
	__builtin_va_start(ap, op);

	/* Determine if the operation requires a third argument */
	switch (op) {
		case F_DUPFD:
		case F_SETFD:
		case F_SETFL:
		case F_SETOWN:
		case F_SETLEASE:
			/* Commands that take an integer or pointer argument */
			arg = __builtin_va_arg(ap, long);
			break;
		case F_GETFD:
		case F_GETFL:
		case F_GETOWN:
		case F_GETLEASE:
			/* Commands that do not take a third argument */
			arg = 0;
			break;
		default:
			/* For unknown commands, assume no argument */
			arg = 0;
			break;
	}

	/* Clean up the variable argument list */
	__builtin_va_end(ap);

	ret = syscall_fcntl(fd, op, arg);

	if (ret < 0) {
		return -1;
	}
	return ret;
}

int connect(int sockfd, const struct sockaddr *addr, unsigned int addrlen) {
	return syscall_connect(sockfd, addr, addrlen);
}
int setsockopt(int sockfd, int level, int optname, const void *optval,
	       unsigned int optlen) {
	return syscall_setsockopt(sockfd, level, optname, optval, optlen);
}
int bind(int sockfd, const struct sockaddr *addr, unsigned int addrlen) {
	return syscall_bind(sockfd, addr, addrlen);
}
int listen(int sockfd, int backlog) { return syscall_listen(sockfd, backlog); }
int getsockname(int sockfd, struct sockaddr *addr, unsigned int *addrlen) {
	return syscall_getsockname(sockfd, addr, addrlen);
}
int accept(int sockfd, struct sockaddr *addr, unsigned int *addrlen) {
	return syscall_accept(sockfd, addr, addrlen);
}
int shutdown(int sockfd, int how) { return syscall_shutdown(sockfd, how); }
int socket(int domain, int type, int protocol) {
	return syscall_socket(domain, type, protocol);
}
int getentropy(void *buffer, size_t length) {
	return syscall_getentropy(buffer, length);
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd,
	   off_t offset) {
	void *ret;
	ret = syscall_mmap(addr, length, prot, flags, fd, offset);

	if ((long)ret == -1) {
		/*err = -(long)ret;*/
		return (void *)-1;
	}
	return ret;
}

int sched_yield(void) { return syscall_sched_yield(); }
int nanosleep(const struct timespec *req, struct timespec *rem) {
	return syscall_nanosleep(req, rem);
}
int gettimeofday(struct timeval *tv, void *tz) {
	return syscall_gettimeofday(tv, tz);
}
int settimeofday(const struct timeval *tv, const void *tz) {
	return syscall_settimeofday(tv, tz);
}
int epoll_create1(int flags) { return syscall_epoll_create1(flags); }
int epoll_wait(int epfd, struct epoll_event *events, int maxevents,
	       int timeout) {
	return syscall_epoll_wait(epfd, events, maxevents, timeout);
}
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
	return syscall_epoll_ctl(epfd, op, fd, event);
}

int open(const char *pathname, int flags, ...) {
	mode_t mode = 0;
	if (flags & 0100 /* O_CREAT */) {
		long arg;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
		__asm__ volatile("movq %1, %0"
				 : "=r"(arg)
				 : "r"(*(long *)(&flags + 1)));
#pragma GCC diagnostic pop
		mode = (mode_t)arg;
	}
	return syscall_open(pathname, flags, mode);
}

off_t lseek(int fd, off_t offset, int whence) {
	return syscall_lseek(fd, offset, whence);
}

#endif /* __linux__ */

