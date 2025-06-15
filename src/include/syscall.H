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
int32_t getpid(void);
int kill(int32_t pid, int signal);
int unlinkat(int dfd, const char *path, int flags);
int64_t write(int fd, const void *buf, uint64_t count);
int64_t read(int fd, void *buf, uint64_t count);
int sched_yield(void);
void exit(int status);
int munmap(void *addr, uint64_t len);
int close(int fd);
int fcntl(int fd, int op, ...);
int clone3(struct clone_args *args, uint64_t size);
int fdatasync(int fd);
int ftruncate(int fd, int64_t length);
int connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
int setsockopt(int sockfd, int level, int optname, const void *optval,
	       uint32_t optlen);
int bind(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
int listen(int sockfd, int backlog);
int getsockname(int sockfd, struct sockaddr *addr, uint32_t *addrlen);
int accept(int sockfd, struct sockaddr *addr, uint32_t *addrlen);
int shutdown(int sockfd, int how);
int socket(int domain, int type, int protocol);
int getrandom(void *buf, uint64_t len, uint32_t flags);
void *mmap(void *addr, uint64_t length, int prot, int flags, int fd,
	   int64_t offset);
int nanosleep(const struct timespec *req, struct timespec *rem);
int gettimeofday(struct timeval *tv, void *tz);
int settimeofday(const struct timeval *tv, const struct timezone *tz);
int epoll_create1(int flags);
int epoll_pwait(int epfd, struct epoll_event *events, int maxevents,
		int timeout, const sigset_t *sigmask, uint64_t size);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int openat(int dfd, const char *pathname, int flags, uint32_t mode);
int64_t lseek(int fd, int64_t offset, int whence);
int setitimer(int32_t which, const struct itimerval *new_value,
	      struct itimerval *old_value);
int rt_sigaction(int signum, const struct rt_sigaction *act,
		 struct rt_sigaction *oldact, uint64_t sigsetsize);
void restorer(void);
int64_t futex(uint32_t *uaddr, int futex_op, uint32_t val,
	   const struct timespec *timeout, uint32_t *uaddr2, uint32_t val3);
int waitid(int32_t int32_type, int32_t id, siginfo_t *sigs, int options);

#endif /* _SYSCALL_H */
