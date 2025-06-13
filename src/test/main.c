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

#include <colors.h>
#include <env.h>
#include <format.h>
#include <misc.h>
#include <test.h>

char **environ = NULL;

int cur_tests = 0;
int exe_test = 0;

TestEntry tests[MAX_TESTS];

void add_test_fn(void (*test_fn)(void), const char *name) {
	if (strlen(name) > MAX_TEST_NAME) panic("test name too long!");
	if (cur_tests >= MAX_TESTS) panic("too many tests!");
	tests[cur_tests].test_fn = test_fn;
	memset(tests[cur_tests].name, 0, MAX_TEST_NAME);
	strcpy(tests[cur_tests].name, name);
	cur_tests++;
}

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);

void call_constructors(void) {
	for (void (**func)(void) = __init_array_start; func < __init_array_end;
	     func++) {
		(*func)();
	}
}

int main(int argc, char **argv, char **envp) {
	int test_count = 0;
	char *tp;

	environ = envp;
	init_environ();

	tp = getenv("TEST_PATTERN");
	if (!tp || !strcmp(tp, "*")) {
		myprintf("Running %d tests...\n", cur_tests);
	} else {
		myprintf("Running test %s...\n", tp);
	}

	for (exe_test = 0; exe_test < cur_tests; exe_test++) {
		if (!tp || !strcmp(tp, "*") ||
		    !strcmp(tests[exe_test].name, tp)) {
			myprintf("running test %d [%s]\n", test_count,
				 tests[exe_test].name);
			tests[exe_test].test_fn();
			test_count++;
		}
	}
	myprintf("%sSuccess%s! %d %stests passed!%s\n", GREEN, RESET,
		 test_count, CYAN, CYAN);

	return 0;
}
