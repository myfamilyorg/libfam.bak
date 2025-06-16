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

#include <types.H>

i32 pipe(i32 fds[2]);
i32 pipe2(i32 fds[2], int flags);
i32 getpid(void);
i32 kill(i32 pid, int signal);
i32 unlinkat(i32 dfd, const u8 *path, int flags);
i64 write(i32 fd, const void *buf, u64 count);
i64 read(i32 fd, void *buf, u64 count);
i32 sched_yield(void);
void exit(i32 status);
i32 munmap(void *addr, u64 len);
i32 close(i32 fd);
i32 fcntl(i32 fd, int op, ...);
i32 clone3(struct clone_args *args, u64 size);
i32 fdatasync(i32 fd);
i32 ftruncate(i32 fd, i64 length);
i32 connect(i32 sockfd, const struct sockaddr *addr, u32 addrlen);
i32 setsockopt(i32 sockfd, int level, int optname, const void *optval,
	       u32 optlen);
i32 bind(i32 sockfd, const struct sockaddr *addr, u32 addrlen);
i32 listen(i32 sockfd, int backlog);
i32 getsockname(i32 sockfd, struct sockaddr *addr, u32 *addrlen);
i32 accept(i32 sockfd, struct sockaddr *addr, u32 *addrlen);
i32 shutdown(i32 sockfd, int how);
i32 socket(i32 domain, int type, int protocol);
i32 getrandom(void *buf, u64 len, u32 flags);
void *mmap(void *addr, u64 length, int prot, int flags, int fd,
	   i64 offset);
i32 nanosleep(const struct timespec *req, struct timespec *rem);
i32 gettimeofday(struct timeval *tv, void *tz);
i32 settimeofday(const struct timeval *tv, const struct timezone *tz);
i32 epoll_create1(i32 flags);
i32 epoll_pwait(i32 epfd, struct epoll_event *events, int maxevents,
		i32 timeout, const sigset_t *sigmask, u64 size);
i32 epoll_ctl(i32 epfd, int op, int fd, struct epoll_event *event);
i32 openat(i32 dfd, const u8 *pathname, int flags, u32 mode);
i64 lseek(i32 fd, i64 offset, int whence);
i32 setitimer(i32 which, const struct itimerval *new_value,
	      struct itimerval *old_value);
i32 rt_sigaction(i32 signum, const struct rt_sigaction *act,
		 struct rt_sigaction *oldact, u64 sigsetsize);
void restorer(void);
i64 futex(u32 *uaddr, int futex_op, u32 val,
	   const struct timespec *timeout, u32 *uaddr2, u32 val3);
i32 waitid(i32 i32ype, i32 id, siginfo_t *sigs, int options);

#endif /* _SYSCALL_H */
