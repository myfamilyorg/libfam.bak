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

#define _GNU_SOURCE

#include <error.h>
#include <fcntl.h>
#include <sys.h>
#include <sys/time.h>
#include <time.h>
#include <types.h>

#ifdef __linux__
#define DECLARE_SYSCALL(ret_type, name, num, ...) \
	ret_type syscall_##name(__VA_ARGS__);     \
	__asm__(".global syscall_" #name          \
		"\n"                              \
		"syscall_" #name                  \
		":\n"                             \
		"    movq $" #num                 \
		", %rax\n"                        \
		"    syscall\n"                   \
		"    ret\n");

void *syscall_mmap(void *addr, size_t length, int prot, int flags, int fd,
		   off_t offset) {
	void *result;
	__asm__ volatile(
	    "movq %1, %%rdi\n"
	    "movq %2, %%rsi\n"
	    "movq %3, %%rdx\n"
	    "movq %4, %%r10\n"
	    "movq %5, %%r8\n"
	    "movq %6, %%r9\n"
	    "movq $9, %%rax\n"
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)
	    : "r"((uint64_t)addr), "r"(length), "r"((uint64_t)prot),
	      "r"((uint64_t)flags), "r"((uint64_t)fd), "r"((uint64_t)offset)
	    : "%rax", "%rdi", "%rsi", "%rdx", "%r10", "%r8", "%r9");
	return result;
}

#define IMPL_WRAPPER(ret_type, name, ...)         \
	ret_type v = syscall_##name(__VA_ARGS__); \
	if (v < 0) {                              \
		err = -v;                         \
		return -1;                        \
	}                                         \
	return v;

DECLARE_SYSCALL(int, sched_yield, 24, void)

DECLARE_SYSCALL(ssize_t, write, 1, int fd, const void *buf, size_t length)

DECLARE_SYSCALL(void, exit, 60, int code)

DECLARE_SYSCALL(int, munmap, 11, void *addr, size_t length)

DECLARE_SYSCALL(int, open, 2, const char *pathname, int flags, ...)

DECLARE_SYSCALL(int, close, 3, int fd)

DECLARE_SYSCALL(int, ftruncate, 77, int fd, long length)

DECLARE_SYSCALL(int, msync, 26, void *addr, unsigned long length, int flags)

DECLARE_SYSCALL(off_t, lseek, 8, int fd, off_t offset, int whence)

DECLARE_SYSCALL(int, fdatasync, 187, int fd)

DECLARE_SYSCALL(int, fork, 57, void)

DECLARE_SYSCALL(int, pipe, 22, int fd[2])

DECLARE_SYSCALL(int, nanosleep, 35, const struct timespec *duration,
		struct timespec *rem)

DECLARE_SYSCALL(int, gettimeofday, 96, struct timeval *tv, void *tz)

DECLARE_SYSCALL(int, settimeofday, 170, const struct timeval *tv,
		const void *tz)

int sys_sched_yield(void) {
	int v = syscall_sched_yield();
	if (v < 0) {
		err = -v;
		return -1;
	}
	return v;
}

ssize_t sys_write(int fd, const void *buf, size_t length) {
	IMPL_WRAPPER(ssize_t, write, fd, buf, length)
}

void sys_exit(int code) {
	syscall_exit(code);
	while (true);
}

void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd,
	       off_t offset) {
	void *v = syscall_mmap(addr, length, prot, flags, fd, offset);
	if ((long)v >= -4095 && (long)v < 0) {
		err = -(long)v;
		return (void *)-1;
	}
	return v;
}

int sys_open(const char *pathname, int flags, ...) {
	IMPL_WRAPPER(int, open, pathname, flags, 0600)
}

int sys_close(int fd) { IMPL_WRAPPER(int, close, fd) }
int sys_ftruncate(int fd, off_t length) {
	IMPL_WRAPPER(int, ftruncate, fd, length)
}
int sys_msync(void *addr, size_t length,
	      int flags){IMPL_WRAPPER(int, msync, addr, length, flags)}

off_t sys_lseek(int fd, off_t offset, int whence) {
	IMPL_WRAPPER(int, lseek, fd, offset, whence)
}

int sys_munmap(void *addr, size_t length) {
	IMPL_WRAPPER(int, munmap, addr, length)
}

int sys_fdatasync(int fd) { IMPL_WRAPPER(int, fdatasync, fd) }

int sys_nanosleep(const struct timespec *duration, struct timespec *buf) {
	IMPL_WRAPPER(int, nanosleep, duration, buf)
}
int sys_gettimeofday(struct timeval *tv, void *tz) {
	IMPL_WRAPPER(int, gettimeofday, tv, tz)
}
int sys_settimeofday(const struct timeval *tv, const struct timezone *tz){
    IMPL_WRAPPER(int, settimeofday, tv, tz)}
#elif defined(__APPLE__)

ssize_t write(int fd, const void *buf, size_t length);
int sched_yield(void);
void exit(int);
void *mmap(void *addr, size_t length, int prot, int flags, int fd,
	   off_t offset);
int munmap(void *addr, size_t length);
int close(int);
int ftruncate(int fd, off_t length);
int msync(void *addr, size_t length, int flags);
off_t lseek(int fd, off_t offset, int whence);
int fdatasync(int fd);
int open(const char *pathname, int flags, ...);
int fork(void);
int pipe(int fds[2]);
int nanosleep(const struct timespec *duration, struct timespec *buf);
int gettimeofday(struct timeval *tv, void *tz);
int settimeofday(const struct timeval *tv, const struct timezone *tz);

ssize_t sys_write(int fd, const void *buf, size_t length) {
	return write(fd, buf, length);
}
int sys_sched_yield(void) { return sched_yield(); }
void sys_exit(int code) { exit(code); }
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd,
	       off_t offset) {
	return mmap(addr, length, prot, flags, fd, offset);
}
int sys_munmap(void *addr, size_t length) { return munmap(addr, length); }
int sys_close(int fd) { return close(fd); }
int sys_ftruncate(int fd, off_t length) { return ftruncate(fd, length); }
int sys_msync(void *addr, size_t length, int flags) {
	return msync(addr, length, flags);
}
off_t sys_lseek(int fd, off_t offset, int whence) {
	return lseek(fd, offset, whence);
}
int sys_fdatasync(int fd) { return fdatasync(fd); }
int sys_fork(void) { return fork(); }
int sys_pipe(int fds[2]) { return pipe(fds); }

int sys_nanosleep(const struct timespec *duration, struct timespec *buf) {
	return nanosleep(duration, buf);
}
int sys_gettimeofday(struct timeval *tv, void *tz) {
	return gettimeofday(tv, tz);
}
int sys_settimeofday(const struct timeval *tv, const struct timezone *tz) {
	return settimeofday(tv, tz);
}
int sys_open(const char *pathname, int flags, ...) {
	return open(pathname, flags, 0644);
}
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

int ocreate(const char *path) {
	return sys_open(path, O_CREAT | O_RDWR, 0644);
}

int64_t micros(void) {
	struct timeval tv;

	// Get current time
	if (sys_gettimeofday(&tv, NULL) == -1) {
		return -1;
	}

	// Calculate microseconds since epoch
	int64_t microseconds = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;

	return microseconds;
}

int sleep_millis(uint64_t millis) {
	struct timespec req;
	req.tv_sec = millis / 1000;
	req.tv_nsec = (millis % 1000) * 1000000;
	return sys_nanosleep(&req, &req);
}

