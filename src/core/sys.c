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

#ifdef __linux__
#define _GNU_SOURCE
#endif /* __linux__ */

#include <sys.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif defined(__APPLE__)
#include <errno.h>
#include <sys/event.h>
#include <unistd.h>
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define STATIC_ASSERT(condition, message) \
	typedef char static_assert_##message[(condition) ? 1 : -1]
#ifdef __linux__
STATIC_ASSERT(sizeof(Event) == sizeof(struct epoll_event), sizes_match);
#endif /* __linux__ */
#ifdef __APPLE__
STATIC_ASSERT(sizeof(Event) == sizeof(struct kevent), sizes_match);
#endif /* __APPLE__ */

#define DEFINE_SYSCALL(ret_type, name, ...)              \
	static ret_type syscall_##name(__VA_ARGS__) {    \
		long result;                             \
		_DEFINE_SYSCALL_INNER(name, __VA_ARGS__) \
		return (ret_type)result;                 \
	}

#define _DEFINE_SYSCALL_INNER(name, ...)                 \
	__asm__ volatile(_SYSCALL_ASM(name, __VA_ARGS__) \
			 : "=r"(result)                  \
			 : _SYSCALL_INPUTS(__VA_ARGS__)  \
			 : "%rax", "%rcx", "%r11",       \
			   "memory" _SYSCALL_CLOBBERS(__VA_ARGS__));

/* Helper to get system call number */
#define _SYSCALL_NUM(name) _syscall_num_##name

/* System call numbers */
#define _syscall_num_write 1
#define _syscall_num_sched_yield 24
#define _syscall_num_open 2
#define _syscall_num_mmap 9

/* Generate assembly for syscall instruction */
#define _SYSCALL_ASM(name, ...) \
	"movq %[sysno], %%rax\n" \
    _SYSCALL_ARGS(__VA_ARGS__) \
    "syscall\n" \
    "movq %%rax, %0\n"

/* Handle arguments for assembly (0 to 6 args) */
#define _SYSCALL_ARGS(...) \
	_SYSCALL_ARGS_INNER(__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0)(__VA_ARGS__)
#define _SYSCALL_ARGS_INNER(_1, _2, _3, _4, _5, _6, N, ...) _SYSCALL_ARGS_##N
#define _SYSCALL_ARGS_0()
#define _SYSCALL_ARGS_1(a) "movq %1, %%rdi\n"
#define _SYSCALL_ARGS_2(a, b) \
	"movq %1, %%rdi\n"    \
	"movq %2, %%rsi\n"
#define _SYSCALL_ARGS_3(a, b, c) \
	"movq %1, %%rdi\n"       \
	"movq %2, %%rsi\n"       \
	"movq %3, %%rdx\n"
#define _SYSCALL_ARGS_4(a, b, c, d) \
	"movq %1, %%rdi\n"          \
	"movq %2, %%rsi\n"          \
	"movq %3, %%rdx\n"          \
	"movq %4, %%r10\n"
#define _SYSCALL_ARGS_5(a, b, c, d, e) \
	"movq %1, %%rdi\n"             \
	"movq %2, %%rsi\n"             \
	"movq %3, %%rdx\n"             \
	"movq %4, %%r10\n"             \
	"movq %5, %%r8\n"
#define _SYSCALL_ARGS_6(a, b, c, d, e, f) \
	"movq %1, %%rdi\n"                \
	"movq %2, %%rsi\n"                \
	"movq %3, %%rdx\n"                \
	"movq %4, %%r10\n"                \
	"movq %5, %%r8\n"                 \
	"movq %6, %%r9\n"

/* Handle input constraints for assembly */
#define _SYSCALL_INPUTS(...) \
	_SYSCALL_INPUTS_INNER(__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0)(__VA_ARGS__)
#define _SYSCALL_INPUTS_INNER(_1, _2, _3, _4, _5, _6, N, ...) \
	_SYSCALL_INPUTS_##N
#define _SYSCALL_INPUTS_0() [sysno] "i"(_SYSCALL_NUM(name))
#define _SYSCALL_INPUTS_1(a) [sysno] "i"(_SYSCALL_NUM(name)), "r"((long)(a))
#define _SYSCALL_INPUTS_2(a, b) \
	[sysno] "i"(_SYSCALL_NUM(name)), "r"((long)(a)), "r"((long)(b))
#define _SYSCALL_INPUTS_3(a, b, c)                                       \
	[sysno] "i"(_SYSCALL_NUM(name)), "r"((long)(a)), "r"((long)(b)), \
	    "r"((long)(c))
#define _SYSCALL_INPUTS_4(a, b, c, d)                                    \
	[sysno] "i"(_SYSCALL_NUM(name)), "r"((long)(a)), "r"((long)(b)), \
	    "r"((long)(c)), "r"((long)(d))
#define _SYSCALL_INPUTS_5(a, b, c, d, e)                                 \
	[sysno] "i"(_SYSCALL_NUM(name)), "r"((long)(a)), "r"((long)(b)), \
	    "r"((long)(c)), "r"((long)(d)), "r"((long)(e))
#define _SYSCALL_INPUTS_6(a, b, c, d, e, f)                              \
	[sysno] "i"(_SYSCALL_NUM(name)), "r"((long)(a)), "r"((long)(b)), \
	    "r"((long)(c)), "r"((long)(d)), "r"((long)(e)), "r"((long)(f))

/* Handle additional clobbered registers based on number of args */
#define _SYSCALL_CLOBBERS(...) \
	_SYSCALL_CLOBBERS_INNER(__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0)(__VA_ARGS__)
#define _SYSCALL_CLOBBERS_INNER(_1, _2, _3, _4, _5, _6, N, ...) \
	_SYSCALL_CLOBBERS_##N
#define _SYSCALL_CLOBBERS_0()
#define _SYSCALL_CLOBBERS_1(a) , "%rdi"
#define _SYSCALL_CLOBBERS_2(a, b) , "%rdi", "%rsi"
#define _SYSCALL_CLOBBERS_3(a, b, c) , "%rdi", "%rsi", "%rdx"
#define _SYSCALL_CLOBBERS_4(a, b, c, d) , "%rdi", "%rsi", "%rdx", "%r10"
#define _SYSCALL_CLOBBERS_5(a, b, c, d, e) \
	, "%rdi", "%rsi", "%rdx", "%r10", "%r8"
#define _SYSCALL_CLOBBERS_6(a, b, c, d, e, f) \
	, "%rdi", "%rsi", "%rdx", "%r10", "%r8", "%r9"

#pragma GCC diagnostic pop
