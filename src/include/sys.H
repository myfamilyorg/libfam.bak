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

#include <syscall.H>
#include <types.H>

/* system calls applied */
int unlink(const u8 *path);
int open(const u8 *pathname, int flags, u32 mode);
int pipe(int fds[2]);
int getentropy(void *buffer, u64 length);
int yield(void);
int timeout(void (*task)(void), u64 milliseconds);
void *map(u64 length);
void *fmap(int fd, i64 size, i64 offset);
void *smap(u64 length);
int exists(const u8 *path);
int file(const u8 *path);
i64 fsize(int fd);
int fresize(int fd, i64 length);
int flush(int fd);
i64 micros(void);
int sleepm(u64 millis);
i32 two(void);
i32 two2(bool share_fds);

#endif /* _SYS_H */
