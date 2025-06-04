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

#include <misc.h>
#include <sys.h>

#define MAX_TESTS 1024
#define MAX_TEST_NAME 128

void add_test_fn(void (*test_fn)(void), const char *name);

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

#define assert_eq(x, y)                                                       \
	if ((x) != (y)) {                                                     \
		const char *msg_pre = "assertion failed in test: [";          \
		const char *msg_post = "].";                                  \
		write(2, msg_pre, strlen(msg_pre));                           \
		write(2, tests[exe_test].name, strlen(tests[exe_test].name)); \
		panic(msg_post);                                              \
	}

#define assert(x) \
	if (!x) panic("assertion failed!");

#endif /* _TEST_H */
