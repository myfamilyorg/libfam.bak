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

#include <error.H>
#include <init.H>
#include <sys.H>
#include <sys.H>
#include <syscall_const.H>

i32 file(const u8 *path) {
	i32 ret = open(path, O_CREAT | O_RDWR, 0600);
	return ret;
}
i32 exists(const u8 *path) {
	i32 fd = open(path, O_RDWR, 0600);
	if (fd > 0) {
		close(fd);
		return 1;
	}
	return 0;
}

i64 micros(void) {
	struct timeval tv;
	if (gettimeofday(&tv, NULL) == -1) return -1;
	return (i64)tv.tv_sec * 1000000 + tv.tv_usec;
}

i32 open(const u8 *path, int flags, u32 mode) {
	return openat(-100, path, flags, mode);
}

i32 unlink(const u8 *path) { return unlinkat(-100, path, 0); }

i32 sleepm(u64 millis) {
	struct timespec req;
	i32 ret;
	req.tv_sec = millis / 1000;
	req.tv_nsec = (millis % 1000) * 1000000;
	ret = nanosleep(&req, &req);
	return ret;
}

i64 fsize(int fd) {
	i64 ret = lseek(fd, 0, SEEK_END);
	return ret;
}

i32 fresize(int fd, i64 length) {
	i32 ret = ftruncate(fd, length);
	return ret;
}

void *map(u64 length) {
	void *v = mmap(NULL, length, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (v == MAP_FAILED) {
		return NULL;
	}
	return v;
}
void *fmap(int fd, i64 size, i64 offset) {
	void *v =
	    mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
	if (v == MAP_FAILED) {
		return NULL;
	}
	return v;
}

void *smap(u64 length) {
	void *v = mmap(NULL, length, PROT_READ | PROT_WRITE,
		       MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	if (v == MAP_FAILED) {
		return NULL;
	}
	return v;
}

i32 flush(int fd) {
	i32 ret = fdatasync(fd);
	return ret;
}

i32 sched_yield(void);
i32 yield(void) {
	i32 ret = sched_yield();
	return ret;
}

i32 pipe(int fds[2]) { return pipe2(fds, 0); }

i32 getentropy(void *buffer, u64 length) {
	return getrandom(buffer, length, GRND_RANDOM);
}
