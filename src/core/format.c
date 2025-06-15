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
#include <format.H>
#include <misc.H>
#include <syscall.H>

typedef long ptrdiff_t;
typedef long intmax_t;
typedef unsigned long uintmax_t;

/* Helper function to reverse a string in place */
static void reverse(char* str, size_t len) {
	size_t i;
	size_t j;
	char tmp;

	for (i = 0, j = len - 1; i < j; i++, j--) {
		tmp = str[i];
		str[i] = str[j];
		str[j] = tmp;
	}
}

/* Helper function to convert an unsigned integer to a string */
static size_t uint_to_str(uintmax_t num, char* buf, int base, int upper) {
	const char* digits;
	size_t i;
	uintmax_t temp;

	digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	i = 0;
	temp = num;

	do {
		buf[i++] = digits[temp % base];
		temp /= base;
	} while (temp);

	reverse(buf, i);
	return i;
}

/* Helper function to convert a signed integer to a string */
static size_t int_to_str(intmax_t num, char* buf, int base, int upper) {
	size_t i;
	intmax_t temp;

	i = 0;
	temp = num;

	if (temp < 0) {
		buf[i++] = '-';
		temp = -temp;
	}

	return i + uint_to_str((uintmax_t)temp, buf + i, base, upper);
}
static int vsnprintf(char* str, size_t size, const char* format,
		     __builtin_va_list ap) {
	const char* fmt;
	size_t pos;
	size_t len;
	char buf[32];
	size_t i;
	size_t j;
	int val;
	unsigned int uval;
	const char* s;
	char c;

	pos = 0;
	len = 0;

	for (fmt = format; *fmt; fmt++) {
		if (*fmt != '%') {
			len++;
			if (str && pos < size) {
				str[pos++] = *fmt;
			}
			continue;
		}
		fmt++; /* Skip '%' */
		if (!*fmt) {
			len++;
			if (str && pos < size) {
				str[pos++] = '%';
			}
			break; /* Handle trailing % */
		}

		switch (*fmt) {
			case 'd':
			case 'i': /* Treat %i the same as %d */
				val = __builtin_va_arg(
				    ap, int); /* Use int for %d and %i */
				j = int_to_str(val, buf, 10, 0);
				len += j;
				for (i = 0; i < j && str && pos < size; i++) {
					str[pos++] = buf[i];
				}
				break;
			case 'u':
				uval = __builtin_va_arg(ap, uint64_t);
				j = uint_to_str(uval, buf, 10, 0);
				len += j;
				for (i = 0; i < j && str && pos < size; i++) {
					str[pos++] = buf[i];
				}
				break;

			case 'x':
				uval = __builtin_va_arg(ap, uint64_t);
				j = uint_to_str(uval, buf, 16, 0);
				len += j;
				for (i = 0; i < j && str && pos < size; i++) {
					str[pos++] = buf[i];
				}
				break;

			case 'X':
				uval = __builtin_va_arg(ap, uint64_t);
				j = uint_to_str(uval, buf, 16, 1);
				len += j;
				for (i = 0; i < j && str && pos < size; i++) {
					str[pos++] = buf[i];
				}
				break;

			case 's':
				s = __builtin_va_arg(ap, const char*);
				if (!s) {
					s = "(null)";
				}
				for (i = 0; s[i]; i++) {
					len++;
					if (str && pos < size) {
						str[pos++] = s[i];
					}
				}
				break;

			case 'c':
				c = (char)__builtin_va_arg(ap, int);
				len++;
				if (str && pos < size) {
					str[pos++] = c;
				}
				break;

			case '%':
				len++;
				if (str && pos < size) {
					str[pos++] = '%';
				}
				break;

			default:
				len++;
				if (str && pos < size) {
					str[pos++] = *fmt;
				}
				break;
		}
	}

	if (size > 0 && str) {
		if (pos + 1 < size) {
			str[pos++] = '\0';
		} else {
			str[size - 1] = '\0';
		}
	}

	return (int)len;
}

/* snprintf implementation that takes variable arguments */
int snprintf(char* str, size_t size, const char* format, ...) {
	__builtin_va_list ap;
	int len;

	__builtin_va_start(ap, format);
	len = vsnprintf(str, size, format, ap);
	__builtin_va_end(ap);
	return len;
}

/* printf implementation */
int printf(const char* format, ...) {
	__builtin_va_list ap, ap_copy;
	int len;
	char* buf;

	__builtin_va_start(ap, format);
	__builtin_va_copy(ap_copy, ap);

	len = vsnprintf(NULL, 0, format, ap);
	if (len > 0) {
		buf = alloc(len + 1);

		if (buf == NULL)
			len = -1;
		else {
			len = vsnprintf(buf, len + 1, format, ap_copy);
			if (len > 0) write(1, buf, len);
			release(buf);
		}
	}

	__builtin_va_end(ap_copy);
	__builtin_va_end(ap);
	return len;
}

void panic(const char* format, ...) {
	__builtin_va_list ap, ap_copy;
	int len;
	char* buf;

	__builtin_va_start(ap, format);
	__builtin_va_copy(ap_copy, ap);

	len = vsnprintf(NULL, 0, format, ap);
	if (len > 0) {
		buf = alloc(len + 1);

		if (buf == NULL)
			len = -1;
		else {
			len = vsnprintf(buf, len + 1, format, ap_copy);
			if (len > 0) {
				write(2, "panic: ", 7);
				write(2, buf, len);
				write(2, "\n", 1);
			}
			release(buf);
		}
	}

	__builtin_va_end(ap_copy);
	__builtin_va_end(ap);
	exit(-1);
}
