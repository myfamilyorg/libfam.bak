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

#ifndef _TEST_H
#define _TEST_H

#include <colors.h>
#include <misc.h>
#include <sys.h>

#define MAX_TESTS 1024
#define MAX_TEST_NAME 128

void add_test_fn(void (*test_fn)(void), const char *name);
static __attribute__((unused)) const char *__assertion_msg =
    "assertion failed in test";

extern int exe_test;
typedef struct {
	void (*test_fn)(void);
	char name[MAX_TEST_NAME + 1];
} TestEntry;
extern TestEntry tests[];

#define Test(name)                                                         \
	void __test_##name(void);                                          \
	static void __attribute__((constructor)) __add_test_##name(void) { \
		add_test_fn(__test_##name, #name);                         \
	}                                                                  \
	void __test_##name(void)

#define ASSERT_EQ(x, y, msg)                                                  \
	if ((x) != (y)) {                                                     \
		write(2, BRIGHT_RED, strlen(BRIGHT_RED));                     \
		write(2, __assertion_msg, strlen(__assertion_msg));           \
		write(2, RESET, strlen(RESET));                               \
		write(2, ": [", 3);                                           \
		write(2, tests[exe_test].name, strlen(tests[exe_test].name)); \
		write(2, "]. ", 3);                                           \
		panic(msg);                                                   \
	}

#define ASSERT(x, msg)                                                        \
	if (!(x)) {                                                           \
		write(2, BRIGHT_RED, strlen(BRIGHT_RED));                     \
		write(2, __assertion_msg, strlen(__assertion_msg));           \
		write(2, RESET, strlen(RESET));                               \
		write(2, ": [", 3);                                           \
		write(2, tests[exe_test].name, strlen(tests[exe_test].name)); \
		write(2, "]. ", 3);                                           \
		panic(msg);                                                   \
	}

#if MEMSAN == 1
#define ASSERT_BYTES(v)                                \
	do {                                           \
		uint64_t _b__ = get_allocated_bytes(); \
		ASSERT_EQ(_b__, v, "ASSERT_BYTES");    \
	} while (0);
#define ASSERT_NOT_BYTES_0() ASSERT(get_allocated_bytes(), "ASSERT_NOT_BYTES")
#else
#define ASSERT_BYTES(v)
#define ASSERT_NOT_BYTES_0()
#endif /* MEMSAN */

#endif /* _TEST_H */
