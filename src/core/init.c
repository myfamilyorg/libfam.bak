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

#include <error.H>
#include <init.H>
#include <types.H>

#define MAX_EXIT 64

static int has_begun = 0;

void begin(void) {
	if (!has_begun) {
		signals_init();
		has_begun = 1;
	}
}

static void (*exit_fns[MAX_EXIT])(void);
static size_t exit_count = 0;

int register_exit(void (*fn)(void)) {
	size_t index = exit_count++;
	if (index >= MAX_EXIT) {
		exit_count--;
		err = ENOSPC;
		return -1;
	}
	exit_fns[index] = fn;
	return 0;
}

void __attribute__((destructor)) execute_exits(void) {
	int i;
	for (i = exit_count - 1; i >= 0; i--) {
		exit_fns[i]();
	}
	exit_count = 0;
}
