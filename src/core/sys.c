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

#include <fcntl.h>
#include <sys.h>
#include <sys/time.h>
#include <time.h>
#include <types.h>

#ifdef __linux__
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
int open_create(const char *path) { return open(path, O_CREAT | O_RDWR, 0644); }

int64_t micros(void) {
	struct timeval tv;

	// Get current time
	if (gettimeofday(&tv, NULL) == -1) {
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
	return nanosleep(&req, &req);
}

#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
