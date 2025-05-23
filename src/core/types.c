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

#include <sys.h>
#include <types.h>

int printf(const char *, ...);

#define CheckType(t, exp)                                                 \
	if (sizeof(t) != exp) {                                           \
		printf("expected sizeof(%s) == %d, Found %lu\n", #t, exp, \
		       sizeof(t));                                        \
		exit(-1);                                                 \
	}

#define CheckEndian()                                                \
	uint16_t test = 0x1234;                                      \
	uint8_t *bytes = (uint8_t *)&test;                           \
	if (bytes[0] != 0x34) {                                      \
		printf("Error: Big-endian systems not supported\n"); \
		exit(-1);                                            \
	}

static __attribute__((constructor)) void check_sizes(void) {
	CheckEndian();
	CheckType(uint8_t, 1);
	CheckType(int8_t, 1);
	CheckType(uint16_t, 2);
	CheckType(int16_t, 2);
	CheckType(uint32_t, 4);
	CheckType(int32_t, 4);
	CheckType(uint64_t, 8);
	CheckType(int64_t, 8);
	CheckType(uint128_t, 16);
	CheckType(int128_t, 16);
	CheckType(byte, 1);
	CheckType(unsigned long, 8);
	CheckType(size_t, 8);
	CheckType(ssize_t, 8);
	if (!true) {
		printf("expected true to be true\n");
		exit(-1);
	}
	if (false) {
		printf("expected false to be false\n");
		exit(-1);
	}
	if (!true) {
		printf("expected true to be true\n");
		exit(-1);
	}
}

