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

#ifndef _SYS_H
#define _SYS_H

#include <types.h>

/* direct system calls */
int pipe2(int fds[2], int flags);
int unlink(const char *path);
ssize_t write(int fd, const void *buf, size_t length);
ssize_t read(int fd, void *buf, size_t length);
void exit(int);
int munmap(void *addr, size_t len);
int close(int fd);
int fcntl(int fd, int op, ...);

/* socket system calls */
int connect(int sockfd, const struct sockaddr *addr, unsigned int addrlen);
int setsockopt(int sockfd, int level, int optname, const void *optval,
	       unsigned int optlen);
int bind(int sockfd, const struct sockaddr *addr, unsigned int addrlen);
int listen(int sockfd, int backlog);
int getsockname(int sockfd, struct sockaddr *addr, unsigned int *addrlen);
int accept(int sockfd, struct sockaddr *addr, unsigned int *addrlen);
int shutdown(int sockfd, int how);
int socket(int domain, int type, int protocol);

/* system calls applied */
int pipe(int fds[2]);
int getentropy(void *buffer, size_t length);
int yield(void);
int timeout(void (*task)(void), uint64_t milliseconds);
void *map(size_t length);
void *fmap(int fd, off_t size, off_t offset);
void *smap(size_t length);
int exists(const char *path);
int file(const char *path);
off_t fsize(int fd);
int fresize(int fd, off_t length);
int flush(int fd);
int64_t micros(void);
int sleepm(uint64_t millis);
pid_t two(void);

#endif /* _SYS_H */
