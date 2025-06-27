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

#ifndef _ATOMIC_H
#define _ATOMIC_H

#include <types.H>

#define __cas128(a, expected, desired)                            \
        __atomic_compare_exchange_n(a, expected, desired, false, \
                                    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)


#ifdef __aarch64__
u32 __cas32(u32 *ptr, u32 *expected, u32 desired);
u32 __and32(u32 *ptr, u32 value);
u32 __add32(u32 *ptr, u32 value);
u32 __sub32(u32 *ptr, u32 value);
u32 __or32(volatile u32 *a, u32 v);

u64 __cas64(u64 *ptr, u64 *expected, u64 desired);
u64 __and64(u64 *ptr, u64 value);
u64 __add64(u64 *ptr, u64 value);
u64 __sub64(u64 *ptr, u64 value);
u64 __or64(volatile u64 *a, u64 v);

#elif defined(__amd64__)

#define __add32(a, v) __atomic_fetch_add(a, v, __ATOMIC_SEQ_CST)
#define __sub32(a, v) __atomic_fetch_sub(a, v, __ATOMIC_SEQ_CST)
#define __or32(a, v) __atomic_or_fetch(a, v, __ATOMIC_SEQ_CST)
#define __and32(a, v) __atomic_and_fetch(a, v, __ATOMIC_SEQ_CST)
#define __cas32(a, expected, desired)                            \
	__atomic_compare_exchange_n(a, expected, desired, false, \
				    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

#define __add64(a, v) __atomic_fetch_add(a, v, __ATOMIC_SEQ_CST)
#define __sub64(a, v) __atomic_fetch_sub(a, v, __ATOMIC_SEQ_CST)
#define __or64(a, v) __atomic_or_fetch(a, v, __ATOMIC_SEQ_CST)
#define __and64(a, v) __atomic_and_fetch(a, v, __ATOMIC_SEQ_CST)
#define __cas64(a, expected, desired)                            \
	__atomic_compare_exchange_n(a, expected, desired, false, \
				    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

#endif

#define ALOAD(a) __atomic_load_n(a, __ATOMIC_ACQUIRE)
#define ASTORE(a, v) __atomic_store_n(a, v, __ATOMIC_RELEASE)

#endif /* _ATOMIC_H */
