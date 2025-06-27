#include <types.H>

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
	if (envp) {
	}
	return 0;
}
