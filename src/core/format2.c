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
#include <format2.H>
#include <misc.H>

i32 format_check_resize(Formatter *f, u64 size) {
	void *tmp;
	if (f->pos + size > f->capacity) {
		tmp = resize(f->buf, f->pos + size);
		if (!tmp) return -1;
		f->capacity = f->pos + size;
		f->buf = tmp;
	}
	return 0;
}

/* Main format_append implementation */
int format_append(Formatter *f, const char *fmt, ...) {
	u8 buf[40] = {0};
	u64 len;
	__builtin_va_list args;
	const char *p, *next_placeholder;
	Printable next;

	/* Check for invalid input */
	if (f == NULL || fmt == NULL) {
		return -1;
	}

	/* Initialize variadic arguments */
	__builtin_va_start(args, fmt);

	/* Parse format string */
	p = fmt;
	while (*p != '\0') {
		next_placeholder = substr(p, "{}");
		if (next_placeholder == NULL) {
			len = strlen(p);
			if (format_check_resize(f, len) < 0) return -1;
			memcpy(f->buf + f->pos, p, len);
			f->pos += len;
			break;
		}

		len = next_placeholder - p;
		if (format_check_resize(f, len) < 0) return -1;
		memcpy(f->buf + f->pos, p, len);
		f->pos += len;

		next = __builtin_va_arg(args, Printable);
		if (next.t == I128_T) {
			len = i128_to_string(buf, next.data.ivalue);
			if (format_check_resize(f, len) < 0) return -1;
			memcpy(f->buf + f->pos, buf, len);
			f->pos += len;
		} else if (next.t == U128_T) {
			len = u128_to_string(buf, next.data.ivalue);
			if (format_check_resize(f, len) < 0) return -1;
			memcpy(f->buf + f->pos, buf, len);
			f->pos += len;
		} else if (next.t == STRING_T) {
			len = strlen(next.data.svalue);
			if (format_check_resize(f, len) < 0) return -1;
			memcpy(f->buf + f->pos, next.data.svalue, len);
			f->pos += len;
		} else {
		}

		/* Move past {} */
		p = next_placeholder + 2;
	}

	/* Cleanup */
	__builtin_va_end(args);
	return 0;
}

/* Return the formatted string */
const u8 *format_to_string(Formatter *f) {
	u8 *result;

	/* Check for invalid input */
	if (f == NULL) {
		return NULL;
	}

	/* Ensure buffer is null-terminated */
	if (f->pos + 1 > f->capacity) {
		result = resize(f->buf, f->pos + 1);
		if (result == NULL) {
			return NULL;
		}
		f->buf = result;
		f->capacity = f->pos + 1;
	}
	f->buf[f->pos] = '\0';

	return f->buf;
}

void format_clear(Formatter *f) {
	if (f->capacity) release(f->buf);
	f->capacity = f->pos = 0;
	f->buf = NULL;
}
