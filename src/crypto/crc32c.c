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

#include <libfam/crc32c.H>
#include <libfam/format.H>
#include <libfam/types.H>

u32 crc32c(const u8* data, u64 length) {
	u32 crc;
	u64 i;
	u32 chunk32;
	u8 byte;

	if (data == (void*)0) return 0;

	crc = 0xFFFFFFFF;

#ifdef __x86_64__
	for (i = 0; i + 4 <= length; i = i + 4) {
		memcpy(&chunk32, data + i, 4);
		__asm__ volatile("crc32l %2, %0"
				 : "=r"(crc)
				 : "0"(crc), "r"(chunk32)
				 : "cc");
	}
	for (; i < length; i = i + 1) {
		byte = data[i];
		__asm__ volatile("crc32b %2, %0"
				 : "=r"(crc)
				 : "0"(crc), "r"((u8)byte)
				 : "cc");
	}
#elif defined(__aarch64__)
	for (i = 0; i + 4 <= length; i = i + 4) {
		memcpy(&chunk32, data + i, 4);
		__asm__ volatile("crc32cw %w0, %w0, %w1"
				 : "=r"(crc)
				 : "r"(chunk32), "0"(crc));
	}
	for (; i < length; i = i + 1) {
		byte = data[i];
		__asm__ volatile("crc32cb %w0, %w0, %w1"
				 : "=r"(crc)
				 : "r"((u8)byte), "0"(crc));
	}
#else
#error Unsupported platform: only x86-64 and ARM64 are supported
#endif

	return crc ^ 0xFFFFFFFF;
}

