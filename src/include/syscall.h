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

#ifndef _SYSCALL_H__
#define _SYSCALL_H__

#include <sys/time.h>
#include <types.h>

struct clone_args;
struct sockaddr;
struct timespec;
struct timeval;
struct epoll_event;
struct timezone;

pid_t fork(void);
int pipe(int fds[2]);
int unlink(const char *path);
ssize_t write(int fd, const void *buf, size_t count);
ssize_t read(int fd, void *buf, size_t count);
int sched_yield(void);
void exit(int status);
int munmap(void *addr, size_t len);
int close(int fd);
int fcntl(int fd, int op, ...);
int clone3(struct clone_args *args, size_t size);
int fdatasync(int fd);
int ftruncate(int fd, off_t length);
int connect(int sockfd, const struct sockaddr *addr, unsigned int addrlen);
int setsockopt(int sockfd, int level, int optname, const void *optval,
	       unsigned int optlen);
int bind(int sockfd, const struct sockaddr *addr, unsigned int addrlen);
int listen(int sockfd, int backlog);
int getsockname(int sockfd, struct sockaddr *addr, unsigned int *addrlen);
int accept(int sockfd, struct sockaddr *addr, unsigned int *addrlen);
int shutdown(int sockfd, int how);
int socket(int domain, int type, int protocol);
int getentropy(void *buf, size_t len);
void *mmap(void *addr, size_t length, int prot, int flags, int fd,
	   off_t offset);
int nanosleep(const struct timespec *req, struct timespec *rem);
int gettimeofday(struct timeval *tv, void *tz);
int settimeofday(const struct timeval *tv, const struct timezone *tz);
int epoll_create1(int flags);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents,
	       int timeout);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int open(const char *pathname, int flags, ...);
off_t lseek(int fd, off_t offset, int whence);
int setitimer(__itimer_which_t which, const struct itimerval *new_value,
	      struct itimerval *old_value);
long futux(uint32_t *uaddr, int futex_op, uint32_t val,
	   const struct timespec *timeout, uint32_t *uaddr2, uint32_t val3);

#endif /* _SYSCALL_H__ */
