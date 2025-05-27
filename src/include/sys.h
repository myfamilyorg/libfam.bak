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

#ifndef _SYS_H__
#define _SYS_H__

#include <types.h>

typedef struct {
#ifdef __linux__
	byte opaque[12];
#elif defined(__APPLE__)
	byte opaque[32];
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
} Event;

struct sockaddr;

#define MULTIPLEX_FLAG_NONE 0
#define MULTIPLEX_FLAG_READ 0x1
#define MULTIPLEX_FLAG_WRITE (0x1 << 1)

/* Unchanged System calls */
pid_t fork(void);
int pipe(int fds[2]);
int unlink(const char *path);
ssize_t write(int fd, const void *buf, size_t length);
ssize_t read(int fd, void *buf, size_t length);
void exit(int);
int munmap(void *addr, size_t len);
int close(int fd);
int fcntl(int fd, int op, ...);
int connect(int sockfd, const struct sockaddr *addr, unsigned int addrlen);
int setsockopt(int sockfd, int level, int optname, const void *optval,
	       unsigned int optlen);
int bind(int sockfd, const struct sockaddr *addr, unsigned int addrlen);
int listen(int sockfd, int backlog);
int getsockname(int sockfd, struct sockaddr *addr, unsigned int *addrlen);
int accept(int sockfd, struct sockaddr *addr, unsigned int *addrlen);
int shutdown(int sockfd, int how);
int socket(int domain, int type, int protocol);

/* Wrapper System calls */
void *map(size_t length);
void *fmap(int fd);
void *smap(size_t length);
int file(const char *path);
off_t fsize(int fd);
int fresize(int fd, off_t length);
int flush(int fd);
int64_t micros(void);
int yield(void);
int sleepm(uint64_t millis);
int multiplex(void);
int mregister(int multiplex, int fd, int flags, void *attach);
int mwait(int multiplex, Event *events, int max_events, int64_t timeout);

int event_getfd(Event event);
int event_is_read(Event event);
int event_is_write(Event event);
void *event_attachment(Event event);

#endif /* _SYS_H__ */
