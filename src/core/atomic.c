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

#include <libfam/types.H>

#ifdef __aarch64__
i32 __cas32(volatile u32 *a, u32 *expected, u32 desired) {
	u32 result;
	i32 success;
	u32 orig_expected = *expected;
	i32 retries = 5;
	while (retries--) {
		__asm__ volatile(
		    "ldaxr %w0, [%2]\n"	   /* Load-exclusive 32-bit */
		    "cmp %w0, %w3\n"	   /* Compare with *expected */
		    "b.ne 1f\n"		   /* Jump to fail if not equal */
		    "stxr w1, %w4, [%2]\n" /* Store-exclusive desired */
		    "cbz w1, 2f\n"     /* Jump to success if store succeeded */
		    "1: mov %w1, #0\n" /* Set success = 0 (fail) */
		    "b 3f\n"
		    "2: mov %w1, #1\n" /* Set success = 1 (success) */
		    "3: dmb ish\n" /* Memory barrier for sequential consistency
				    */
		    : "=&r"(result), "=&r"(success)
		    : "r"(a), "r"(*expected), "r"(desired)
		    : "x0", "x1", "memory");
		if (success) break;
		*expected = result;
		if (result != orig_expected) break;
	}
	return success;
}

u32 __add32(volatile u32 *a, u32 v) {
	u32 old, tmp;
	__asm__ volatile(
	    "1: ldaxr %w0, [%2]\n" /* Load-exclusive 32-bit */
	    "add %w1, %w0, %w3\n"  /* Compute new value */
	    "stxr w4, %w1, [%2]\n" /* Store-exclusive */
	    "cbnz w4, 1b\n"	   /* Retry if store failed */
	    "dmb ish\n"		   /* Memory barrier */
	    : "=&r"(old), "=&r"(tmp), "+r"(a)
	    : "r"(v)
	    : "x4", "memory");
	return old;
}

u32 __sub32(volatile u32 *a, u32 v) { return __add32(a, -v); }

u32 __or32(volatile u32 *a, u32 v) {
	u32 old, tmp;
	__asm__ volatile(
	    "1: ldaxr %w0, [%2]\n" /* Load-exclusive 32-bit */
	    "orr %w1, %w0, %w3\n"  /* Bitwise OR */
	    "stxr w4, %w1, [%2]\n" /* Store-exclusive */
	    "cbnz w4, 1b\n"	   /* Retry if store failed */
	    "dmb ish\n"		   /* Memory barrier */
	    : "=&r"(old), "=&r"(tmp), "+r"(a)
	    : "r"(v)
	    : "x4", "memory");
	return old;
}

u32 __and32(volatile u32 *a, u32 v) {
	u32 old, tmp;
	__asm__ volatile(
	    "1: ldaxr %w0, [%2]\n" /* Load-exclusive 32-bit */
	    "and %w1, %w0, %w3\n"  /* Bitwise AND */
	    "stxr w4, %w1, [%2]\n" /* Store-exclusive */
	    "cbnz w4, 1b\n"	   /* Retry if store failed */
	    "dmb ish\n"		   /* Memory barrier */
	    : "=&r"(old), "=&r"(tmp), "+r"(a)
	    : "r"(v)
	    : "x4", "memory");
	return old;
}

i32 __cas64(volatile u64 *a, u64 *expected, u64 desired) {
	u64 result;
	i32 success;
	u64 orig_expected = *expected;
	i32 retries = 5;
	while (retries--) {
		__asm__ volatile(
		    "ldaxr %0, [%2]\n"
		    "cmp %0, %3\n"
		    "b.ne 1f\n"
		    "stxr w1, %4, [%2]\n"
		    "cbz w1, 2f\n"
		    "1: mov %w1, #0\n"
		    "b 3f\n"
		    "2: mov %w1, #1\n"
		    "3: dmb ish\n"
		    : "=&r"(result), "=&r"(success)
		    : "r"(a), "r"(*expected), "r"(desired)
		    : "x0", "x1", "memory");
		if (success) break;
		*expected = result;
		if (result != orig_expected) break;
	}
	return success;
}

u64 __add64(volatile u64 *a, u64 v) {
	u64 old, tmp;
	__asm__ volatile(
	    "1: ldaxr %0, [%2]\n" /* Load-exclusive 64-bit */
	    "add %1, %0, %3\n"	  /* Compute new value */
	    "stxr w4, %1, [%2]\n" /* Store-exclusive */
	    "cbnz w4, 1b\n"	  /* Retry if store failed */
	    "dmb ish\n"		  /* Memory barrier */
	    : "=&r"(old), "=&r"(tmp), "+r"(a)
	    : "r"(v)
	    : "x4", "memory");
	return old;
}

u64 __sub64(volatile u64 *a, u64 v) { return __add64(a, -v); }

u64 __or64(volatile u64 *a, u64 v) {
	u64 old, tmp;
	__asm__ volatile(
	    "1: ldaxr %0, [%2]\n" /* Load-exclusive 64-bit */
	    "orr %1, %0, %3\n"	  /* Bitwise OR (64-bit) */
	    "stxr w4, %1, [%2]\n" /* Store-exclusive 64-bit */
	    "cbnz w4, 1b\n"	  /* Retry if store failed */
	    "dmb ish\n"		  /* Memory barrier */
	    : "=&r"(old), "=&r"(tmp), "+r"(a)
	    : "r"(v)
	    : "x4", "memory");
	return old;
}

u64 __and64(volatile u64 *a, u64 v) {
	u64 old, tmp;
	__asm__ volatile(
	    "1: ldaxr %0, [%2]\n" /* Load-exclusive 64-bit */
	    "and %1, %0, %3\n"	  /* Bitwise AND (64-bit) */
	    "stxr w4, %1, [%2]\n" /* Store-exclusive 64-bit */
	    "cbnz w4, 1b\n"	  /* Retry if store failed */
	    "dmb ish\n"		  /* Memory barrier */
	    : "=&r"(old), "=&r"(tmp), "+r"(a)
	    : "r"(v)
	    : "x4", "memory");
	return old;
}

#elif defined(__amd64__)
i32 __cas32(volatile u32 *a, u32 *expected, u32 desired) {
	u32 result = *expected;
	__asm__ volatile("lock cmpxchgl %2, %1"
		     : "+a"(result), "+m"(*a)
		     : "r"(desired)
		     : "cc", "memory");
	i32 success = (result == *expected);
	if (!success) {
		*expected = result;
	}
	return success;
}
u32 __add32(volatile u32 *a, u32 v) {
	u32 old = v;
	__asm__ volatile("lock xaddl %0, %1"
		     : "+r"(old), "+m"(*a)
		     :
		     : "cc", "memory");
	return old;
}
u32 __sub32(volatile u32 *a, u32 v) { return __add32(a, -v); }
u32 __or32(volatile u32 *a, u32 v) {
	u32 old;
	__asm__ volatile(
	    "movl %1, %%eax\n"
	    "1:\n"
	    "movl %%eax, %%edx\n"
	    "orl %2, %%edx\n"
	    "lock cmpxchgl %%edx, %1\n"
	    "jne 1b\n"
	    : "=a"(old), "+m"(*a)
	    : "r"(v)
	    : "cc", "memory", "rdx");
	return old;
}
u32 __and32(volatile u32 *a, u32 v) {
	u32 old;
	__asm__ volatile(
	    "movl %1, %%eax\n"
	    "1:\n"
	    "movl %%eax, %%edx\n"
	    "andl %2, %%edx\n"
	    "lock cmpxchgl %%edx, %1\n"
	    "jne 1b\n"
	    : "=a"(old), "+m"(*a)
	    : "r"(v)
	    : "cc", "memory", "rdx");
	return old;
}
i32 __cas64(volatile u64 *a, u64 *expected, u64 desired) {
	u64 result = *expected;
	__asm__ volatile("lock cmpxchgq %2, %1"
		     : "+a"(result), "+m"(*a)
		     : "r"(desired)
		     : "cc", "memory");
	i32 success = (result == *expected);
	if (!success) {
		*expected = result;
	}
	return success;
}
u64 __add64(volatile u64 *a, u64 v) {
	u64 old = v;
	__asm__ volatile("lock xaddq %0, %1"
		     : "+r"(old), "+m"(*a)
		     :
		     : "cc", "memory");
	return old;
}
u64 __sub64(volatile u64 *a, u64 v) { return __add64(a, -v); }
u64 __or64(volatile u64 *a, u64 v) {
	u64 old;
	__asm__ volatile(
	    "movq %1, %%rax\n"
	    "1:\n"
	    "movq %%rax, %%rdx\n"
	    "orq %2, %%rdx\n"
	    "lock cmpxchgq %%rdx, %1\n"
	    "jne 1b\n"
	    : "=a"(old), "+m"(*a)
	    : "r"(v)
	    : "cc", "memory", "rdx");
	return old;
}
u64 __and64(volatile u64 *a, u64 v) {
	u64 old;
	__asm__ volatile(
	    "movq %1, %%rax\n"
	    "1:\n"
	    "movq %%rax, %%rdx\n"
	    "andq %2, %%rdx\n"
	    "lock cmpxchgq %%rdx, %1\n"
	    "jne 1b\n"
	    : "=a"(old), "+m"(*a)
	    : "r"(v)
	    : "cc", "memory", "rdx");
	return old;
}

#endif
