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

#ifndef _ALLOC_H__
#define _ALLOC_H__

#include <sys.h>
#include <types.h>

#ifndef CHUNK_SIZE
#define CHUNK_SIZE (0x1 << 22) /* 4mb */
#endif

#ifndef MAX_SLAB_SIZE
#define MAX_SLAB_SIZE (CHUNK_SIZE >> 2) /* 1mb */
#endif

#ifndef MIN_ALIGN_SIZE
#define MIN_ALIGN_SIZE (0x1 << 14) /* 16kb */
#endif

#ifndef MEM_SAN
#define MEM_SAN 0 /* Disabled by default */
#endif

void *alloc(size_t size);
void release(void *ptr);
void *resize(void *ptr, size_t size);

void ga_init();

#if MEM_SAN == 1
uint64_t get_mmaped_bytes();
#endif /* MEM_SAN */

#endif /* _ALLOC_H__ */
