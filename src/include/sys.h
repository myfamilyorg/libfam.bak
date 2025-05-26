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

#ifndef _FAM_H__
#define _FAM_H__

#include <types.h>

ssize_t sys_write(int fd, const void *buf, size_t length);
int sys_sched_yield(void);
void sys_exit(int);
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd,
	       off_t offset);
int sys_munmap(void *addr, size_t length);
int sys_close(int fd);
int sys_ftruncate(int fd, off_t length);
int sys_msync(void *addr, size_t length, int flags);
off_t sys_lseek(int fd, off_t offset, int whence);
int sys_fdatasync(int fd);
int sys_fork(void);
int sys_pipe(int fds[2]);

int ocreate(const char *path);
int64_t micros(void);
int sleep_millis(uint64_t millis);

#endif /* _FAM_H__ */
