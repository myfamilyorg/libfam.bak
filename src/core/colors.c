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
#include <misc.h>
#include <types.h>

int no_color() { return getenv("NO_COLOR") != NULL; }

const char *get_dimmed() {
	if (no_color()) {
		return "";
	} else {
		return "\x1b[2m";
	}
}

const char *get_red() {
	if (no_color()) {
		return "";
	} else {
		return "\x1b[31m";
	}
}

const char *get_bright_red() {
	if (no_color()) {
		return "";
	} else {
		return "\x1b[91m";
	}
}

const char *get_green() {
	if (no_color()) {
		return "";
	} else {
		return "\x1b[32m";
	}
}

const char *get_yellow() {
	if (no_color()) {
		return "";
	} else {
		return "\x1b[33m";
	}
}

const char *get_cyan() {
	if (no_color()) {
		return "";
	} else {
		return "\x1b[36m";
	}
}

const char *get_magenta() {
	if (no_color()) {
		return "";
	} else {
		return "\x1b[35m";
	}
}

const char *get_blue() {
	if (no_color()) {
		return "";
	} else {
		return "\x1b[34m";
	}
}

const char *get_reset() {
	if (no_color()) {
		return "";
	} else {
		return "\x1b[0m";
	}
}
