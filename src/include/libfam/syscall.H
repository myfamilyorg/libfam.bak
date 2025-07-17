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

#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <libfam/types.H>

i32 pipe(i32 fds[2]);
i32 pipe2(i32 fds[2], i32 flags);
i32 getpid(void);
i32 kill(i32 pid, i32 signal);
i32 unlinkat(i32 dfd, const u8 *path, i32 flags);
i64 write(i32 fd, const void *buf, u64 count);
i64 read(i32 fd, void *buf, u64 count);
i32 sched_yield(void);
void exit(i32 status);
i32 munmap(void *addr, u64 len);
i32 close(i32 fd);
i32 fcntl(i32 fd, i32 op, ...);
i32 clone3(struct clone_args *args, u64 size);
i32 fdatasync(i32 fd);
i32 ftruncate(i32 fd, i64 length);
i32 connect(i32 sockfd, const struct sockaddr *addr, u32 addrlen);
i32 setsockopt(i32 sockfd, i32 level, i32 optname, const void *optval,
	       u32 optlen);
i32 getsockopt(i32 sockfd, i32 level, i32 optname, void *optval, u32 *optlen);
i32 bind(i32 sockfd, const struct sockaddr *addr, u32 addrlen);
i32 listen(i32 sockfd, i32 backlog);
i32 getsockname(i32 sockfd, struct sockaddr *addr, u32 *addrlen);
i32 accept(i32 sockfd, struct sockaddr *addr, u32 *addrlen);
i32 shutdown(i32 sockfd, i32 how);
i32 socket(i32 domain, i32 type, i32 protocol);
i32 getrandom(void *buf, u64 len, u32 flags);
void *mmap(void *addr, u64 length, i32 prot, i32 flags, i32 fd, i64 offset);
i32 nanosleep(const struct timespec *req, struct timespec *rem);
i32 gettimeofday(struct timeval *tv, void *tz);
i32 settimeofday(const struct timeval *tv, const struct timezone *tz);
i32 epoll_create1(i32 flags);
i32 epoll_pwait(i32 epfd, struct epoll_event *events, i32 maxevents,
		i32 timeout, const sigset_t *sigmask, u64 size);
i32 epoll_ctl(i32 epfd, i32 op, i32 fd, struct epoll_event *event);
i32 openat(i32 dfd, const u8 *pathname, i32 flags, u32 mode);
i64 lseek(i32 fd, i64 offset, i32 whence);
i32 setitimer(i32 which, const struct itimerval *new_value,
	      struct itimerval *old_value);
i32 rt_sigaction(i32 signum, const struct rt_sigaction *act,
		 struct rt_sigaction *oldact, u64 sigsetsize);
void restorer(void);
i64 futex(u32 *uaddr, i32 futex_op, u32 val, const struct timespec *timeout,
	  u32 *uaddr2, u32 val3);
i32 waitid(i32 i32ype, i32 id, siginfo_t *sigs, i32 options);
i32 execve(const u8 *pathname, u8 *const argv[], u8 *const envp[]);

#if TEST == 1
extern bool _debug_no_write;
extern bool _debug_fail_getsockbyname;
extern bool _debug_fail_listen;
extern bool _debug_fail_setsockopt;
extern bool _debug_fail_epoll_create1;
extern bool _debug_fail_fcntl;
extern bool _debug_fail_pipe2;
#endif /* TEST */

#endif /* _SYSCALL_H */
