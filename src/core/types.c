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

#include <libfam/format.H>
#include <libfam/misc.H>
#include <libfam/syscall.H>
#include <libfam/types.H>

void __stack_chk_fail() { panic("STACKCHECKFAIL!"); }

#define CheckEndian()                                                        \
	u16 test = 0x1234;                                                   \
	u8 *u8s = (u8 *)&test;                                               \
	if (u8s[0] != 0x34) {                                                \
		const u8 *msg = "Error: Big-endian systems not supported\n"; \
		i32 v = write(2, msg, strlen(msg));                          \
		if (v) {                                                     \
		}                                                            \
		exit(-1);                                                    \
	}

#define STATIC_ASSERT(condition, message) \
	typedef u8 static_assert_##message[(condition) ? 1 : -1]

STATIC_ASSERT(sizeof(u8) == 1, u8_sizes_match);
STATIC_ASSERT(sizeof(i8) == 1, i8_sizes_match);
STATIC_ASSERT(sizeof(u16) == 2, u16_sizes_match);
STATIC_ASSERT(sizeof(i16) == 2, i16_sizes_match);
STATIC_ASSERT(sizeof(u32) == 4, u32_sizes_match);
STATIC_ASSERT(sizeof(i32) == 4, i32_sizes_match);
STATIC_ASSERT(sizeof(u64) == 8, u64_sizes_match);
STATIC_ASSERT(sizeof(i64) == 8, i64_sizes_match);
STATIC_ASSERT(sizeof(u128) == 16, u128_sizes_match);
STATIC_ASSERT(sizeof(i128) == 16, i128_sizes_match);
STATIC_ASSERT(sizeof(i64) == 8, ssize_sizes_match);
STATIC_ASSERT(sizeof(u64) == 8, size_sizes_match);

static __attribute__((constructor)) void check_sizes(void) { CheckEndian(); }
