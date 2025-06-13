#include <colors.h>
#include <env.h>
#include <misc.h>
#include <stdio.h>
#include <test.h>

char **environ = NULL;

int cur_tests = 0;
int exe_test = 0;

TestEntry tests[MAX_TESTS];

int printf(const char *x __attribute__((unused)), ...) { return 0; }

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

int main(int argc, char *argv[], char *envp[]);

#ifdef __aarch64__
__asm__(
    ".section .text\n"
    ".global _start\n"
    "_start:\n"
    "    ldr x0, [sp]          // Load argc from stack\n"
    "    add x1, sp, #8        // argv is at sp + 8\n"
    "    add x3, x0, #1        // x3 = argc + 1 (for argv null terminator)\n"
    "    lsl x3, x3, #3        // x3 = (argc + 1) * 8 (byte offset)\n"
    "    add x2, x1, x3        // envp = argv + (argc + 1) * 8\n"
    "    sub sp, sp, x3        // Align stack to 16 bytes\n"
    "    stp x0, x1, [sp, #-16]! // Save argc, argv\n"
    "    str x2, [sp, #-16]!     // Save envp\n"
    "    bl call_constructors    // Call constructors\n"
    "    ldr x2, [sp], #16       // Restore envp\n"
    "    ldp x0, x1, [sp], #16   // Restore argc, argv\n"
    "    bl main                 // Call main\n"
    "    mov x8, #93             // Syscall number for exit (Linux ARM64)\n"
    "    svc #0                  // Invoke exit syscall\n");
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
    "    push %rdi\n"
    "    push %rsi\n"
    "    push %rdx\n"
    "    call call_constructors\n"
    "    pop %rdx\n"
    "    pop %rsi\n"
    "    pop %rdi\n"
    "    call main\n"
    "    mov $60, %rax\n"
    "    mov %rax, %rdi\n"
    "    syscall\n");
#endif /* __amd64__ */

int main(int argc, char **argv, char **envp) {
	int test_count = 0;
	char *tp;

	environ = envp;
	init_environ();

	tp = getenv("TEST_PATTERN");
	if (!tp || !strcmp(tp, "*")) {
		printf("Running %i tests...\n", cur_tests);
	} else {
		printf("Running test %s...\n", tp);
	}

	for (exe_test = 0; exe_test < cur_tests; exe_test++) {
		if (!tp || !strcmp(tp, "*") ||
		    !strcmp(tests[exe_test].name, tp)) {
			printf("running test %i [%s]\n", test_count,
			       tests[exe_test].name);
			write(2, "run: ", 5);
			write(2, tests[exe_test].name,
			      strlen(tests[exe_test].name));
			write(2, "\n", 1);
			tests[exe_test].test_fn();
			test_count++;
		}
	}
	write(2, "Success!\n", strlen("Success!\n"));
	printf("%sSuccess%s! %i %stests passed!%s\n", GREEN, RESET, test_count,
	       CYAN, CYAN);
	exit(0);
	return 0;
}
