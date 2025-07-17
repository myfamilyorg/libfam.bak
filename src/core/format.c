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

#include <libfam/alloc.H>
#include <libfam/format.H>
#include <libfam/misc.H>

STATIC i32 format_check_resize(Formatter *f, u64 size) {
	void *tmp;
	if (f->pos + size > f->capacity) {
		tmp = resize(f->buf, f->pos + size);
		if (!tmp) return -1;
		f->capacity = f->pos + size;
		f->buf = tmp;
	}
	return 0;
}

STATIC const char *find_next_placeholder(const char *p) {
	const char *v1 = substr(p, "{x}");
	const char *v2 = substr(p, "{X}");
	const char *v3 = substr(p, "{c}");
	const char *v4 = substr(p, "{}");

	/* If no placeholders found, return NULL */
	if (!v1 && !v2 && !v3 && !v4) return NULL;

	/* Initialize result to NULL; update with earliest non-NULL pointer */
	const char *result = NULL;
	if (v1 && (!result || v1 < result)) result = v1;
	if (v2 && (!result || v2 < result)) result = v2;
	if (v3 && (!result || v3 < result)) result = v3;
	if (v4 && (!result || v4 < result)) result = v4;

	return result;
}

/* Main format_append implementation */
i32 format_append(Formatter *f, const char *fmt, ...) {
	u8 buf[64] = {0};
	bool hex = false, upper = false, is_char = false;
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
		next_placeholder = find_next_placeholder(p);
		if (next_placeholder == NULL) {
			len = strlen(p);
			if (format_check_resize(f, len) < 0) return -1;
			memcpy(f->buf + f->pos, p, len);
			f->pos += len;
			break;
		}
		if (next_placeholder[1] == 'X') {
			hex = true;
			upper = true;
			is_char = false;
		} else if (next_placeholder[1] == 'x') {
			hex = true;
			upper = false;
			is_char = false;
		} else if (next_placeholder[1] == 'c') {
			hex = false;
			is_char = true;
		} else {
			hex = false;
			upper = false;
			is_char = false;
		}

		len = next_placeholder - p;
		if (format_check_resize(f, len) < 0) return -1;
		memcpy(f->buf + f->pos, p, len);
		f->pos += len;

		next = __builtin_va_arg(args, Printable);
		if (is_char && next.t == U128_T) {
			if (format_check_resize(f, 1) < 0) return -1;
			f->buf[f->pos++] = next.data.uvalue;
		} else if (is_char && next.t == I128_T) {
			if (format_check_resize(f, 1) < 0) return -1;
			f->buf[f->pos++] = next.data.ivalue;
		} else if (next.t == I128_T) {
			len = i128_to_string_impl(buf, next.data.ivalue, hex,
						  upper);
			if (format_check_resize(f, len) < 0) return -1;
			memcpy(f->buf + f->pos, buf, len);
			f->pos += len;
		} else if (next.t == U128_T) {
			len = u128_to_string_impl(buf, next.data.ivalue, hex,
						  upper);
			if (format_check_resize(f, len) < 0) return -1;
			memcpy(f->buf + f->pos, buf, len);
			f->pos += len;
		} else if (next.t == STRING_T) {
			len = strlen(next.data.svalue);
			if (format_check_resize(f, len) < 0) return -1;
			memcpy(f->buf + f->pos, next.data.svalue, len);
			f->pos += len;
		}

		/* Move past {} */
		if (next_placeholder[1] == '}')
			p = next_placeholder + 2;
		else
			p = next_placeholder + 3;
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
