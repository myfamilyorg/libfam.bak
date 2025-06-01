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
#endif

#define STATIC_ASSERT(condition, message) \
	typedef char static_assert_##message[(condition) ? 1 : -1]

#include <error.h>
#include <fcntl.h>
#include <signal.h>
#include <sys.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/epoll.h>
#include <sys/random.h>
STATIC_ASSERT(sizeof(Event) == sizeof(struct epoll_event), event_match);
#elif defined(__APPLE__)
int sched_yield(void);
int fdatasync(int);
#include <dispatch/dispatch.h>
#include <errno.h>
#include <sys/event.h>
#include <unistd.h>
STATIC_ASSERT(sizeof(Event) == sizeof(struct kevent), event_match);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

#define SET_ERR             \
	if (ret < 0) {      \
		err = -ret; \
		return -1;  \
	}                   \
	return ret;

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

/* Declare execute_exits */
void execute_exits();

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
DEFINE_SYSCALL3(318, int, getrandom, void *, buffer, size_t, length,
		unsigned int, flags)
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
DEFINE_SYSCALL1(75, int, fdatasync, int, fd)
DEFINE_SYSCALL2(77, int, ftruncate, int, fd, off_t, length)
DEFINE_SYSCALL1(37, unsigned int, alarm, unsigned int, seconds)
DEFINE_SYSCALL3(38, int, setitimer, __itimer_which_t, which,
		const struct itimerval *, new_value, struct itimerval *,
		old_value)
DEFINE_SYSCALL2(48, sighandler_t, signal, int, signum, sighandler_t,
		signal_handler)

static int syscall_sigaction_rt(int signum, const struct kernel_sigaction *act,
				struct kernel_sigaction *oldact, size_t sz) {
	long result;
	__asm__ volatile(
	    "movq $13"
	    ", %%rax\n"
	    "movq %1, %%rdi\n"
	    "movq %2, %%rsi\n"
	    "movq %3, %%rdx\n"
	    "movq %4, %%r10\n"
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)
	    : "r"((long)(signum)), "r"((long)(act)), "r"((long)(oldact)),
	      "r"((long)(sz))
	    : "%rax", "%rcx", "%r11", "%rdi", "%rsi", "%rdx", "%r10", "memory");
	return result;
}

pid_t fork(void) {
	int ret = syscall_fork();
	SET_ERR
}
int pipe(int fds[2]) {
	int ret = syscall_pipe(fds);
	SET_ERR
}
int unlink(const char *path) {
	int ret = syscall_unlink(path);
	SET_ERR
}
ssize_t write(int fd, const void *buf, size_t count) {
	ssize_t ret = syscall_write(fd, buf, count);
	SET_ERR
}
ssize_t read(int fd, void *buf, size_t count) {
	ssize_t ret = syscall_read(fd, buf, count);
	SET_ERR
}

void exit(int status) {
	execute_exits();
	syscall_exit(status);
	while (true)
		;
}
int munmap(void *addr, size_t len) {
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

	SET_ERR
}

int fdatasync(int fd) {
	int ret = syscall_fdatasync(fd);
	SET_ERR
}

int ftruncate(int fd, off_t length) {
	int ret = syscall_ftruncate(fd, length);
	SET_ERR
}

int connect(int sockfd, const struct sockaddr *addr, unsigned int addrlen) {
	int ret = syscall_connect(sockfd, addr, addrlen);
	SET_ERR
}
int setsockopt(int sockfd, int level, int optname, const void *optval,
	       unsigned int optlen) {
	int ret = syscall_setsockopt(sockfd, level, optname, optval, optlen);
	SET_ERR
}
int bind(int sockfd, const struct sockaddr *addr, unsigned int addrlen) {
	int ret = syscall_bind(sockfd, addr, addrlen);
	SET_ERR
}
int listen(int sockfd, int backlog) {
	int ret = syscall_listen(sockfd, backlog);
	SET_ERR
}
int getsockname(int sockfd, struct sockaddr *addr, unsigned int *addrlen) {
	int ret = syscall_getsockname(sockfd, addr, addrlen);
	SET_ERR
}
int accept(int sockfd, struct sockaddr *addr, unsigned int *addrlen) {
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

int getentropy(void *buf, size_t len) {
	size_t total;
	if (len > 256) {
		err = EIO; /* getentropy limits to 256 uint8_ts */
		return -1;
	}
	if (!buf) {
		err = EFAULT;
		return -1;
	}

	total = 0;
	while (total < len) {
		ssize_t ret = syscall_getrandom(buf, len, GRND_RANDOM);

		if (ret < 0) {
			err = -ret;
			return -1;
		}
		total += ret;
	}
	return 0;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd,
	   off_t offset) {
	void *ret;
	ret = syscall_mmap(addr, length, prot, flags, fd, offset);

	if ((long)ret == -1) {
		err = -(long)ret;
		return (void *)-1;
	}
	return ret;
}

int sched_yield(void) {
	int ret = syscall_sched_yield();
	SET_ERR
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
int epoll_wait(int epfd, struct epoll_event *events, int maxevents,
	       int timeout) {
	int ret = syscall_epoll_wait(epfd, events, maxevents, timeout);
	SET_ERR
}
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
	int ret = syscall_epoll_ctl(epfd, op, fd, event);
	SET_ERR
}

int open(const char *pathname, int flags, ...) {
	mode_t mode = 0;
	int ret;
	if (flags & 0100 /* O_CREAT */) {
		__builtin_va_list ap;
		__builtin_va_start(ap, flags);
		mode = __builtin_va_arg(ap, mode_t);
		__builtin_va_end(ap);
	}
	ret = syscall_open(pathname, flags, mode);
	SET_ERR
}

off_t lseek(int fd, off_t offset, int whence) {
	off_t ret = syscall_lseek(fd, offset, whence);
	SET_ERR
}

int sigaction_rt(int signum, const struct kernel_sigaction *act,
		 struct kernel_sigaction *oldact) {
	int ret = syscall_sigaction_rt(signum, act, oldact, 8);
	SET_ERR
}

sighandler_t signal(int signum, sighandler_t handler) {
	sighandler_t ret = syscall_signal(signum, handler);
	return ret;
}

unsigned int alarm(unsigned int seconds) {
	int ret = syscall_alarm(seconds);
	SET_ERR
}

int setitimer(__itimer_which_t which, const struct itimerval *new_value,
	      struct itimerval *old_value) {
	int ret = syscall_setitimer(which, new_value, old_value);
	SET_ERR
}

#endif /* __linux__ */

/* System calls with changes */
int multiplex(void) {
#ifdef __linux__
	return epoll_create1(0);
#elif defined(__APPLE__)
	int ret = kqueue();
	if (ret == -1) err = errno;
	return ret;
#endif
}

int mregister(int multiplex, int fd, int flags, void *attach) {
#ifdef __linux__
	struct epoll_event ev;
	int event_flags = 0;

	if (flags & MULTIPLEX_FLAG_READ) {
		event_flags |= EPOLLIN;
	}

	if (flags & MULTIPLEX_FLAG_WRITE) {
		event_flags |= EPOLLOUT;
	}

	ev.events = event_flags;
	if (attach == NULL)
		ev.data.fd = fd;
	else
		ev.data.ptr = attach;

	if (epoll_ctl(multiplex, EPOLL_CTL_ADD, fd, &ev) < 0) {
		if (err == EEXIST) {
			if (epoll_ctl(multiplex, EPOLL_CTL_MOD, fd, &ev) < 0) {
				return -1;
			}
		} else {
			return -1;
		}
	}

	return 0;
#elif defined(__APPLE__)
	struct kevent change_event[2];
	int event_count = 0, res;

	if (flags & MULTIPLEX_FLAG_READ) {
		EV_SET(&change_event[event_count], fd, EVFILT_READ,
		       EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, attach);
		event_count++;
	}

	if (flags & MULTIPLEX_FLAG_WRITE) {
		EV_SET(&change_event[event_count], fd, EVFILT_WRITE,
		       EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, attach);
		event_count++;
	}

	res = kevent(multiplex, change_event, event_count, NULL, 0, NULL);
	if (res < 0) err = errno;
	if (res < 0) return -1;
	return 0;
#endif
}

static int has_begun = 0;

void begin(void) {
	if (!has_begun) {
		has_begun = 1;
	}
}

int mwait(int multiplex, Event *events, int max_events,
	  int64_t timeout_millis) {
#ifdef __linux__
	int timeout = (timeout_millis >= 0) ? (int)timeout_millis : -1;

	return epoll_wait(multiplex, (struct epoll_event *)events, max_events,
			  timeout);
#elif defined(__APPLE__)
	struct timespec ts;
	struct timespec *timeout_ptr = NULL;
	int ret;

	if (timeout_millis >= 0) {
		ts.tv_sec = timeout_millis / 1000;
		ts.tv_nsec = (timeout_millis % 1000) * 1000000;
		timeout_ptr = &ts;
	}

	ret = kevent(multiplex, NULL, 0, (struct kevent *)events, max_events,
		     timeout_ptr);
	if (ret == -1) err = errno;
	return ret;
#endif
}

int event_is_read(Event event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->events & EPOLLIN;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)&event;
	return kv->filter == EVFILT_READ;
#endif
}

int event_is_write(Event event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->events & EPOLLOUT;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)&event;
	return kv->filter == EVFILT_WRITE;
#endif
}

void *event_attachment(Event event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->data.ptr;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)&event;
	return kv->udata;
#endif
}

int file(const char *path) {
	int ret = open(path, O_CREAT | O_RDWR, 0600);
#ifdef __APPLE__
	if (ret == -1) err = errno;
#endif
	return ret;
}
int exists(const char *path) {
	int fd = open(path, O_RDWR);
	if (fd > 0) {
		close(fd);
		return 1;
	}
	return 0;
}

int64_t micros(void) {
	struct timeval tv;

	if (gettimeofday(&tv, NULL) == -1) {
#ifdef __APPLE__
		err = errno;
#endif
		return -1;
	}

	return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

int sleepm(uint64_t millis) {
	struct timespec req;
	int ret;
	req.tv_sec = millis / 1000;
	req.tv_nsec = (millis % 1000) * 1000000;
	ret = nanosleep(&req, &req);
#ifdef __APPLE__
	if (ret == -1) err = errno;
#endif
	return ret;
}

off_t fsize(int fd) {
	off_t ret = lseek(fd, 0, SEEK_END);
#ifdef __APPLE__
	err = errno;
#endif
	return ret;
}

int yield(void) {
	int ret = sched_yield();
#ifdef __APPLE__
	if (ret == -1) err = errno;
#endif
	return ret;
}
int fresize(int fd, off_t length) {
	int ret = ftruncate(fd, length);
#ifdef __APPLE__
	if (ret == -1) err = errno;
#endif

	return ret;
}

void *map(size_t length) {
	void *v = mmap(NULL, length, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (v == MAP_FAILED) {
#ifdef __APPLE__
		err = errno;
#endif
		return NULL;
	}
	return v;
}
void *fmap(int fd, off_t offset, off_t size) {
	void *v =
	    mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
	if (v == MAP_FAILED) {
#ifdef __APPLE__
		err = errno;
#endif

		return NULL;
	}
	return v;
}
void *smap(size_t length) {
	void *v = mmap(NULL, length, PROT_READ | PROT_WRITE,
		       MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	if (v == MAP_FAILED) {
#ifdef __APPLE__
		err = errno;
#endif
		return NULL;
	}
	return v;
}

pid_t two(void) {
	pid_t pid = fork();
	if (pid == 0) begin();
#ifdef __APPLE__
	if (pid == -1) err = errno;
#endif
	return pid;
}

int flush(int fd) {
	int ret = fdatasync(fd);
#ifdef __APPLE__
	if (ret == -1) err = errno;
#endif
	return ret;
}
