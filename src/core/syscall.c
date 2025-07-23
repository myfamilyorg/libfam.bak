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

#include <libfam/error.H>
#include <libfam/init.H>
#include <libfam/sys.H>
#include <libfam/syscall_const.H>
#include <libfam/types.H>

PUBLIC bool _debug_no_write = false;
PUBLIC bool _debug_no_exit = false;
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
#define SYS_pipe2 59
#define SYS_unlinkat 35
#define SYS_write 64
#define SYS_read 63
#define SYS_munmap 215
#define SYS_close 57
#define SYS_fcntl 25
#define SYS_connect 203
#define SYS_setsockopt 208
#define SYS_getsockopt 209
#define SYS_bind 200
#define SYS_listen 201
#define SYS_getsockname 204
#define SYS_accept 202
#define SYS_shutdown 210
#define SYS_socket 198
#define SYS_getrandom 278
#define SYS_mmap 222
#define SYS_nanosleep 101
#define SYS_sched_yield 124
#define SYS_gettimeofday 169
#define SYS_settimeofday 170
#define SYS_epoll_create1 20
#define SYS_epoll_pwait 22
#define SYS_epoll_ctl 21
#define SYS_openat 56
#define SYS_lseek 62
#define SYS_fdatasync 83
#define SYS_ftruncate 46
#define SYS_setitimer 103
#define SYS_clone3 435
#define SYS_futex 98
#define SYS_rt_sigaction 134
#define SYS_getpid 172
#define SYS_kill 129
#define SYS_execve 221
#define SYS_msync 227
#define SYS_writev 66
#define SYS_pread64 67
#define SYS_pwrite64 68
#define SYS_waitid 95

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

static __inline__ i64 raw_syscall(i64 sysno, i64 a0, i64 a1, i64 a2, i64 a3,
				  i64 a4, i64 a5) {
	i64 result;
	__asm__ volatile(
	    "mov x8, %1\n"
	    "mov x0, %2\n"
	    "mov x1, %3\n"
	    "mov x2, %4\n"
	    "mov x3, %5\n"
	    "mov x4, %6\n"
	    "mov x5, %7\n"
	    "svc #0\n"
	    "mov %0, x0\n"
	    : "=r"(result)
	    : "r"(sysno), "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5)
	    : "x0", "x1", "x2", "x3", "x4", "x5", "x8", "memory");
	return result;
}

#elif defined(__amd64__)
#define SYS_pipe2 293
#define SYS_unlinkat 263
#define SYS_write 1
#define SYS_read 0
#define SYS_munmap 11
#define SYS_close 3
#define SYS_fcntl 72
#define SYS_connect 42
#define SYS_setsockopt 54
#define SYS_getsockopt 55
#define SYS_bind 49
#define SYS_listen 50
#define SYS_getsockname 51
#define SYS_accept 43
#define SYS_shutdown 48
#define SYS_socket 41
#define SYS_getrandom 318
#define SYS_mmap 9
#define SYS_nanosleep 35
#define SYS_sched_yield 24
#define SYS_gettimeofday 96
#define SYS_settimeofday 164
#define SYS_epoll_create1 291
#define SYS_epoll_pwait 281
#define SYS_epoll_ctl 233
#define SYS_openat 257
#define SYS_lseek 8
#define SYS_fdatasync 75
#define SYS_ftruncate 77
#define SYS_setitimer 38
#define SYS_clone3 435
#define SYS_futex 202
#define SYS_rt_sigaction 13
#define SYS_getpid 39
#define SYS_kill 62
#define SYS_execve 59
#define SYS_msync 26
#define SYS_writev 20
#define SYS_pread64 17
#define SYS_pwrite64 18
#define SYS_waitid 247

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

static __inline__ i64 raw_syscall(i64 sysno, i64 a1, i64 a2, i64 a3, i64 a4,
				  i64 a5, i64 a6) {
	register i64 _a4 __asm__("r10") = a4;
	register i64 _a5 __asm__("r8") = a5;
	register i64 _a6 __asm__("r9") = a6;
	i64 result;
	__asm__ volatile("syscall"
			 : "=a"(result)
			 : "a"(sysno), "D"(a1), "S"(a2), "d"(a3), "r"(_a4),
			   "r"(_a5), "r"(_a6)
			 : "rcx", "r11", "memory");
	return result;
}

#endif /* Arch */

static __inline__ i32 syscall_pipe2(i32 *fds, i32 flags) {
	return (i32)raw_syscall(SYS_pipe2, (i64)fds, (i64)flags, 0, 0, 0, 0);
}
static __inline__ i32 syscall_unlinkat(i32 dfd, const u8 *path, i32 flags) {
	return (i32)raw_syscall(SYS_unlinkat, (i64)dfd, (i64)path, (i64)flags,
				0, 0, 0);
}
static __inline__ i64 syscall_write(i32 fd, const void *buf, u64 count) {
	return raw_syscall(SYS_write, (i64)fd, (i64)buf, (i64)count, 0, 0, 0);
}
static __inline__ i64 syscall_read(i32 fd, void *buf, u64 count) {
	return raw_syscall(SYS_read, (i64)fd, (i64)buf, (i64)count, 0, 0, 0);
}
static __inline__ i32 syscall_munmap(void *addr, u64 len) {
	return (i32)raw_syscall(SYS_munmap, (i64)addr, (i64)len, 0, 0, 0, 0);
}
static __inline__ i32 syscall_close(i32 fd) {
	return (i32)raw_syscall(SYS_close, (i64)fd, 0, 0, 0, 0, 0);
}
static __inline__ i32 syscall_fcntl(i32 fd, i32 cmd, i64 arg) {
	return (i32)raw_syscall(SYS_fcntl, (i64)fd, (i64)cmd, (i64)arg, 0, 0,
				0);
}
static __inline__ i32 syscall_connect(i32 sockfd, const struct sockaddr *addr,
				      u32 addrlen) {
	return (i32)raw_syscall(SYS_connect, (i64)sockfd, (i64)addr,
				(i64)addrlen, 0, 0, 0);
}
static __inline__ i32 syscall_setsockopt(i32 sockfd, i32 level, i32 optname,
					 const void *optval, u32 optlen) {
	return (i32)raw_syscall(SYS_setsockopt, (i64)sockfd, (i64)level,
				(i64)optname, (i64)optval, (i64)optlen, 0);
}
static __inline__ i32 syscall_getsockopt(i32 sockfd, i32 level, i32 optname,
					 void *optval, u32 *optlen) {
	return (i32)raw_syscall(SYS_getsockopt, (i64)sockfd, (i64)level,
				(i64)optname, (i64)optval, (i64)optlen, 0);
}
static __inline__ i32 syscall_bind(i32 sockfd, const struct sockaddr *addr,
				   u32 addrlen) {
	return (i32)raw_syscall(SYS_bind, (i64)sockfd, (i64)addr, (i64)addrlen,
				0, 0, 0);
}
static __inline__ i32 syscall_listen(i32 sockfd, i32 backlog) {
	return (i32)raw_syscall(SYS_listen, (i64)sockfd, (i64)backlog, 0, 0, 0,
				0);
}
static __inline__ i32 syscall_getsockname(i32 sockfd, struct sockaddr *addr,
					  u32 *addrlen) {
	return (i32)raw_syscall(SYS_getsockname, (i64)sockfd, (i64)addr,
				(i64)addrlen, 0, 0, 0);
}
static __inline__ i32 syscall_accept(i32 sockfd, struct sockaddr *addr,
				     u32 *addrlen) {
	return (i32)raw_syscall(SYS_accept, (i64)sockfd, (i64)addr,
				(i64)addrlen, 0, 0, 0);
}
static __inline__ i32 syscall_shutdown(i32 sockfd, i32 how) {
	return (i32)raw_syscall(SYS_shutdown, (i64)sockfd, (i64)how, 0, 0, 0,
				0);
}
static __inline__ i32 syscall_socket(i32 domain, i32 type, i32 protocol) {
	return (i32)raw_syscall(SYS_socket, (i64)domain, (i64)type,
				(i64)protocol, 0, 0, 0);
}
static __inline__ i32 syscall_getrandom(void *buffer, u64 length, u32 flags) {
	return (i32)raw_syscall(SYS_getrandom, (i64)buffer, (i64)length,
				(i64)flags, 0, 0, 0);
}
static __inline__ void *syscall_mmap(void *addr, u64 length, i32 prot,
				     i32 flags, i32 fd, i64 offset) {
	return (void *)(u64)raw_syscall(SYS_mmap, (i64)addr, (i64)length,
					(i64)prot, (i64)flags, (i64)fd,
					(i64)offset);
}
static __inline__ i32 syscall_nanosleep(const struct timespec *req,
					struct timespec *rem) {
	return (i32)raw_syscall(SYS_nanosleep, (i64)req, (i64)rem, 0, 0, 0, 0);
}
static __inline__ i32 syscall_sched_yield(void) {
	return (i32)raw_syscall(SYS_sched_yield, 0, 0, 0, 0, 0, 0);
}
static __inline__ i32 syscall_gettimeofday(struct timeval *tv, void *tz) {
	return (i32)raw_syscall(SYS_gettimeofday, (i64)tv, (i64)tz, 0, 0, 0, 0);
}
static __inline__ i32 syscall_settimeofday(const struct timeval *tv,
					   const void *tz) {
	return (i32)raw_syscall(SYS_settimeofday, (i64)tv, (i64)tz, 0, 0, 0, 0);
}
static __inline__ i32 syscall_epoll_create1(i32 flags) {
	return (i32)raw_syscall(SYS_epoll_create1, (i64)flags, 0, 0, 0, 0, 0);
}
static __inline__ i32 syscall_epoll_pwait(i32 epfd, struct epoll_event *events,
					  i32 maxevents, i32 timeout,
					  const sigset_t *sigs, i32 size) {
	return (i32)raw_syscall(SYS_epoll_pwait, (i64)epfd, (i64)events,
				(i64)maxevents, (i64)timeout, (i64)sigs,
				(i64)size);
}
static __inline__ i32 syscall_epoll_ctl(i32 epfd, i32 op, i32 fd,
					struct epoll_event *event) {
	return (i32)raw_syscall(SYS_epoll_ctl, (i64)epfd, (i64)op, (i64)fd,
				(i64)event, 0, 0);
}
static __inline__ i32 syscall_openat(i32 dfd, const u8 *pathname, i32 flags,
				     u32 mode) {
	return (i32)raw_syscall(SYS_openat, (i64)dfd, (i64)pathname, (i64)flags,
				(i64)mode, 0, 0);
}
static __inline__ i64 syscall_lseek(i32 fd, i64 offset, i32 whence) {
	return raw_syscall(SYS_lseek, (i64)fd, (i64)offset, (i64)whence, 0, 0,
			   0);
}
static __inline__ i32 syscall_fdatasync(i32 fd) {
	return (i32)raw_syscall(SYS_fdatasync, (i64)fd, 0, 0, 0, 0, 0);
}
static __inline__ i32 syscall_ftruncate(i32 fd, i64 length) {
	return (i32)raw_syscall(SYS_ftruncate, (i64)fd, (i64)length, 0, 0, 0,
				0);
}
static __inline__ i32 syscall_setitimer(i32 which,
					const struct itimerval *new_value,
					struct itimerval *old_value) {
	return (i32)raw_syscall(SYS_setitimer, (i64)which, (i64)new_value,
				(i64)old_value, 0, 0, 0);
}
static __inline__ i32 syscall_clone3(struct clone_args *args, u64 size) {
	return (i32)raw_syscall(SYS_clone3, (i64)args, (i64)size, 0, 0, 0, 0);
}
static __inline__ i64 syscall_futex(u32 *uaddr, i32 futex_op, u32 val,
				    const struct timespec *timeout, u32 *uaddr2,
				    u32 val3) {
	return raw_syscall(SYS_futex, (i64)uaddr, (i64)futex_op, (i64)val,
			   (i64)timeout, (i64)uaddr2, (i64)val3);
}
static __inline__ i32 syscall_rt_sigaction(i32 signum,
					   const struct rt_sigaction *act,
					   struct rt_sigaction *oldact,
					   u64 sigsetsize) {
	return (i32)raw_syscall(SYS_rt_sigaction, (i64)signum, (i64)act,
				(i64)oldact, (i64)sigsetsize, 0, 0);
}
static __inline__ i32 syscall_getpid(void) {
	return (i32)raw_syscall(SYS_getpid, 0, 0, 0, 0, 0, 0);
}
static __inline__ i32 syscall_kill(i32 pid, i32 signal) {
	return (i32)raw_syscall(SYS_kill, (i64)pid, (i64)signal, 0, 0, 0, 0);
}
static __inline__ i32 syscall_execve(const u8 *pathname, u8 *const *argv,
				     u8 *const *envp) {
	return (i32)raw_syscall(SYS_execve, (i64)pathname, (i64)argv, (i64)envp,
				0, 0, 0);
}
static __inline__ i32 syscall_msync(void *addr, u64 length, i32 flags) {
	return (i32)raw_syscall(SYS_msync, (i64)addr, (i64)length, (i64)flags,
				0, 0, 0);
}
static __inline__ i64 syscall_writev(i32 fd, const struct iovec *iov,
				     i32 iovcnt) {
	return raw_syscall(SYS_writev, (i64)fd, (i64)iov, (i64)iovcnt, 0, 0, 0);
}
static __inline__ i64 syscall_pread64(i32 fd, void *buf, u64 count,
				      i64 offset) {
	return raw_syscall(SYS_pread64, (i64)fd, (i64)buf, (i64)count,
			   (i64)offset, 0, 0);
}
static __inline__ i64 syscall_pwrite64(i32 fd, const void *buf, u64 count,
				       i64 offset) {
	return raw_syscall(SYS_pwrite64, (i64)fd, (i64)buf, (i64)count,
			   (i64)offset, 0, 0);
}
static __inline__ i32 syscall_waitid(i32 idtype, i32 id, siginfo_t *infop,
				     i32 options) {
	return (i32)raw_syscall(SYS_waitid, (i64)idtype, (i64)id, (i64)infop,
				(i64)options, 0, 0);
}

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

PUBLIC i64 write(i32 fd, const void *buf, u64 count) {
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

PUBLIC void exit(i32 status) {
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
	if (ret < 0) perror("close");
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

i32 waitid(i32 idtype, i32 id, siginfo_t *sigs, i32 options) {
	i32 ret = syscall_waitid(idtype, id, sigs, options);
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

i32 execve(const u8 *pathname, u8 *const argv[], u8 *const envp[]) {
	i32 ret = syscall_execve(pathname, argv, envp);
	SET_ERR
}

i64 writev(i32 fd, const struct iovec *iov, i32 iovcnt) {
	i64 ret = syscall_writev(fd, iov, iovcnt);
	SET_ERR
}
i64 pread(i32 fd, void *buf, u64 count, i64 offset) {
	i64 ret = syscall_pread64(fd, buf, count, offset);
	SET_ERR
}
i64 pwrite(i32 fd, const void *buf, u64 count, i64 offset) {
	i64 ret = syscall_pwrite64(fd, buf, count, offset);
	SET_ERR
}
i32 msync(void *addr, u64 length, i32 flags) {
	i32 ret = syscall_msync(addr, length, flags);
	SET_ERR
}
