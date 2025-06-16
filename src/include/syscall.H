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

int pipe(int fds[2]);
int pipe2(int fds[2], int flags);
i32 getpid(void);
int kill(i32 pid, int signal);
int unlinkat(int dfd, const u8 *path, int flags);
i64 write(int fd, const void *buf, u64 count);
i64 read(int fd, void *buf, u64 count);
int sched_yield(void);
void exit(int status);
int munmap(void *addr, u64 len);
int close(int fd);
int fcntl(int fd, int op, ...);
int clone3(struct clone_args *args, u64 size);
int fdatasync(int fd);
int ftruncate(int fd, i64 length);
int connect(int sockfd, const struct sockaddr *addr, u32 addrlen);
int setsockopt(int sockfd, int level, int optname, const void *optval,
	       u32 optlen);
int bind(int sockfd, const struct sockaddr *addr, u32 addrlen);
int listen(int sockfd, int backlog);
int getsockname(int sockfd, struct sockaddr *addr, u32 *addrlen);
int accept(int sockfd, struct sockaddr *addr, u32 *addrlen);
int shutdown(int sockfd, int how);
int socket(int domain, int type, int protocol);
int getrandom(void *buf, u64 len, u32 flags);
void *mmap(void *addr, u64 length, int prot, int flags, int fd,
	   i64 offset);
int nanosleep(const struct timespec *req, struct timespec *rem);
int gettimeofday(struct timeval *tv, void *tz);
int settimeofday(const struct timeval *tv, const struct timezone *tz);
int epoll_create1(int flags);
int epoll_pwait(int epfd, struct epoll_event *events, int maxevents,
		int timeout, const sigset_t *sigmask, u64 size);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int openat(int dfd, const u8 *pathname, int flags, u32 mode);
i64 lseek(int fd, i64 offset, int whence);
int setitimer(i32 which, const struct itimerval *new_value,
	      struct itimerval *old_value);
int rt_sigaction(int signum, const struct rt_sigaction *act,
		 struct rt_sigaction *oldact, u64 sigsetsize);
void restorer(void);
i64 futex(u32 *uaddr, int futex_op, u32 val,
	   const struct timespec *timeout, u32 *uaddr2, u32 val3);
int waitid(i32 i32ype, i32 id, siginfo_t *sigs, int options);

#endif /* _SYSCALL_H */
