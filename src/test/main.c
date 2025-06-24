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

#include <alloc.H>
#include <env.H>
#include <format2.H>
#include <misc.H>
#include <sys.H>
#include <test.H>

u8 **environ = 0;
i32 cur_tests = 0;
i32 exe_test = 0;

TestEntry tests[MAX_TESTS];

void add_test_fn(void (*test_fn)(void), const u8 *name) {
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
	void (**func)(void);
	for (func = __init_array_start; func < __init_array_end; func++) {
		(*func)();
	}
}

#ifndef COVERAGE
i32 main(i32 argc, u8 *argv[], u8 *envp[]);

#ifdef __aarch64__
__asm__(
    ".section .text\n"
    ".global _start\n"
    "_start:\n"
    "    ldr x0, [sp]\n"
    "    add x1, sp, #8\n"
    "    add x3, x0, #1\n"
    "    lsl x3, x3, #3\n"
    "    add x2, x1, x3\n"
    "    sub sp, sp, x3\n"
    "    bl main\n"
    "    mov x8, #93\n"
    "    svc #0\n");
#endif /* __aarch64__ */

#ifdef __amd64__
__asm__(
    ".section .text\n"
    ".global _start\n"
    "_start:\n"
    "    movq (%rsp), %rdi\n"
    "    lea 8(%rsp), %rsi\n"
    "    mov %rdi, %rcx\n"
    "    add $1, %rcx\n"
    "    shl $3, %rcx\n"
    "    lea (%rsi, %rcx), %rdx\n"
    "    mov %rsp, %rcx\n"
    "    and $-16, %rsp\n"
    "    call main\n"
    "    mov %rax, %rdi\n"
    "    mov $60, %rax\n"
    "    syscall\n");
#endif /* __amd64__ */
#endif /* COVERAGE */

i32 main(i32 argc __attribute__((unused)), u8 **argv __attribute__((unused)),
	 u8 **envp) {
	i32 test_count = 0, len;
	u8 *tp;
	u64 total;
	u8 buf[128];
	double ms;

	reset_allocated_bytes();
#ifndef COVERAGE
	call_constructors();
#endif

	environ = envp;
	init_environ();

	tp = getenv("TEST_PATTERN");
	if (!tp || !strcmp(tp, "*")) {
		println("{}Running {} tests{} ...", CYAN, cur_tests, RESET);
	} else {
		println("{}Running test{}: '{}' ...", CYAN, RESET, tp);
	}

	println(
	    "------------------------------------------------------------------"
	    "--------------------------");

	total = micros();

	for (exe_test = 0; exe_test < cur_tests; exe_test++) {
		if (!tp || !strcmp(tp, "*") ||
		    !strcmp(tests[exe_test].name, tp)) {
			u64 start;
			print("{}Running test{} {} [{}{}{}]", YELLOW, RESET,
			      1 + test_count, DIMMED, tests[exe_test].name,
			      RESET);
			start = micros();
			tests[exe_test].test_fn();
			println(" {}[{} {}s]{}", GREEN, (i32)(micros() - start),
				"µ", RESET);
			test_count++;
		}
	}

	ms = (double)(micros() - total) / (double)1000;

	println(
	    "------------------------------------------------------------------"
	    "--------------------------");
	len = double_to_string(buf, ms, 3);
	buf[len] = 0;
	println("{}Success{}! {} {}tests passed!{} {}[{} ms]{}", GREEN, RESET,
		test_count, CYAN, RESET, GREEN, buf, RESET);

	return 0;
}
