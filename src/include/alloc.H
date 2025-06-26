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

#ifndef _ALLOC_H
#define _ALLOC_H

#ifndef MEMSAN
#define MEMSAN 0 /* Disabled by default */
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE ((u64)(4 * 4096))
#endif /* PAGE_SIZE */

#ifndef CHUNK_SIZE
#define CHUNK_SIZE ((u64)(0x1 << 22)) /* 4mb */
#endif

#ifndef MAX_SLAB_SIZE
#define MAX_SLAB_SIZE ((u64)(CHUNK_SIZE >> 2)) /* 1mb */
#endif

#include <sys.H>
#include <types.H>

typedef struct Alloc Alloc;

typedef enum { ALLOC_TYPE_MAP, ALLOC_TYPE_SMAP, ALLOC_TYPE_FMAP } AllocType;

Alloc *alloc_init(AllocType t, u64 size, ...);
void alloc_destroy(Alloc *a);
void *alloc_impl(Alloc *a, u64 size);
void release_impl(Alloc *a, void *ptr);
void *resize_impl(Alloc *a, void *ptr, u64 size);

void *alloc(u64 size);
void release(void *ptr);
void *resize(void *ptr, u64 size);

u64 allocated_bytes_impl(Alloc *a);
void reset_allocated_bytes_impl(Alloc *a);
u64 get_allocated_bytes(void);
void reset_allocated_bytes(void);

#if TEST == 1
extern u64 _debug_cas_loop;
extern u64 _debug_alloc_failure;
extern u64 _debug_alloc_failure_bypass_count;
#endif /* TEST */

#endif /* _ALLOC_H */
