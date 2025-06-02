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

#ifdef __linux__
#define _GNU_SOURCE
#endif /* __linux__ */
#include <sys.h>
#include <sys/mman.h>
#ifdef __APPLE__
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
int fdatasync(int fd);
#endif /* __APPLE__ */
#include <error.h>
#include <fcntl.h>
#include <robust.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static int has_begun = 0;

void begin(void) {
	if (!has_begun) {
		signals_init();
		robust_init();
		has_begun = 1;
	}
}

int file(const char *path) {
	int ret = open(path, O_CREAT | O_RDWR, 0600);
#ifdef __APPLE__
	if (ret == -1) err = errno;
#endif
	return ret;
}
int exists(const char *path) {
	int fd = open(path, O_RDWR);
	if (fd > 0) {
		close(fd);
		return 1;
	}
	return 0;
}

int64_t micros(void) {
	struct timeval tv;

	if (gettimeofday(&tv, NULL) == -1) {
#ifdef __APPLE__
		err = errno;
#endif
		return -1;
	}

	return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

int sleepm(uint64_t millis) {
	struct timespec req;
	int ret;
	req.tv_sec = millis / 1000;
	req.tv_nsec = (millis % 1000) * 1000000;
	ret = nanosleep(&req, &req);
#ifdef __APPLE__
	if (ret == -1) err = errno;
#endif
	return ret;
}

off_t fsize(int fd) {
	off_t ret = lseek(fd, 0, SEEK_END);
#ifdef __APPLE__
	err = errno;
#endif
	return ret;
}

int fresize(int fd, off_t length) {
	int ret = ftruncate(fd, length);
#ifdef __APPLE__
	if (ret == -1) err = errno;
#endif

	return ret;
}

void *map(size_t length) {
	void *v = mmap(NULL, length, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (v == MAP_FAILED) {
#ifdef __APPLE__
		err = errno;
#endif
		return NULL;
	}
	return v;
}
void *fmap(int fd, off_t offset, off_t size) {
	void *v =
	    mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
	if (v == MAP_FAILED) {
#ifdef __APPLE__
		err = errno;
#endif

		return NULL;
	}
	return v;
}

void *smap(size_t length) {
	void *v = mmap(NULL, length, PROT_READ | PROT_WRITE,
		       MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	if (v == MAP_FAILED) {
#ifdef __APPLE__
		err = errno;
#endif
		return NULL;
	}
	return v;
}

pid_t two(void) {
	pid_t pid = fork();
	if (pid == 0) begin();
#ifdef __APPLE__
	if (pid == -1) err = errno;
#endif
	return pid;
}

int flush(int fd) {
	int ret = fdatasync(fd);
#ifdef __APPLE__
	if (ret == -1) err = errno;
#endif
	return ret;
}

int sched_yield(void);
int yield(void) {
	int ret = sched_yield();
#ifdef __APPLE__
	if (ret == -1) err = errno;
#endif
	return ret;
}
