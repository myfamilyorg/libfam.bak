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

#include <types.h>

uint32_t __cas32(uint32_t *ptr, uint32_t *expected, uint32_t desired);
uint32_t __and32(uint32_t *ptr, uint32_t value);
uint32_t __add32(uint32_t *ptr, uint32_t value);
uint32_t __sub32(uint32_t *ptr, uint32_t value);
uint32_t __or32(volatile uint32_t *a, uint32_t v);

uint64_t __cas64(uint64_t *ptr, uint64_t *expected, uint64_t desired);
uint64_t __and64(uint64_t *ptr, uint64_t value);
uint64_t __add64(uint64_t *ptr, uint64_t value);
uint64_t __sub64(uint64_t *ptr, uint64_t value);
uint64_t __or64(volatile uint64_t *a, uint64_t v);

#define ALOAD(a) __atomic_load_n(a, __ATOMIC_ACQUIRE)
#define ASTORE(a, v) __atomic_store_n(a, v, __ATOMIC_RELEASE)

#endif /* _ATOMIC_H */
