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

#include <misc.h>
#include <types.h>

// Constants
#define O_RDWR 0x0002
#define O_CREAT 0x0040
#define MODE_0644 0644

size_t strlen(const char *X) {
	const char *Y;
	if (X == NULL) return 0;
	Y = X;
	while (*X) X++;
	return X - Y;
}

int strcmp(const char *X, const char *Y) {
	if (X == NULL || Y == NULL) {
		if (X == Y) return 0;
		return X == NULL ? -1 : 1;
	}
	while (*X == *Y && *X) {
		X++;
		Y++;
	}
	if ((byte)*X > (byte)*Y) return 1;
	if ((byte)*Y > (byte)*X) return -1;
	return 0;
}

int strcmpn(const char *X, const char *Y, size_t n) {
	while (n > 0 && *X == *Y && *X != '\0' && *Y != '\0') {
		X++;
		Y++;
		n--;
	}
	if (n == 0) return 0;
	return (byte)*X - (byte)*Y;
}

char *strstr(const char *s, const char *sub) {
	if (s == NULL || sub == NULL) return NULL;
	for (; *s; s++) {
		const char *tmps = s, *tmpsub = sub;
		while (*(byte *)tmps == *(byte *)tmpsub && *tmps) {
			tmps++;
			tmpsub++;
		}
		if (*tmpsub == '\0') return (char *)s;
	}
	return NULL;
}

void *memset(void *dest, int c, size_t n) {
	byte *s = (byte *)dest;
	size_t i;

	if (dest == NULL || n == 0) {
		return dest;
	}

	for (i = 0; i < n; i++) {
		s[i] = (byte)c;
	}
	return dest;
}

void *memcpy(void *dest, const void *src, size_t n) {
	byte *d = (byte *)dest;
	const byte *s = (byte *)src;
	size_t i;

	if (dest == NULL || src == NULL || n == 0) {
		return dest;
	}

	for (i = 0; i < n; i++) {
		d[i] = s[i];
	}
	return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
	byte *d = (byte *)dest;
	const byte *s = (byte *)src;
	size_t i;

	if (dest == NULL || src == NULL || n == 0) {
		return dest;
	}

	if (d <= s || d >= s + n) {
		for (i = 0; i < n; i++) {
			d[i] = s[i];
		}
	} else {
		for (i = n; i > 0; i--) {
			d[i - 1] = s[i - 1];
		}
	}

	return dest;
}

void bzero(void *s, size_t len) { memset(s, 0, len); }

size_t uint128_t_to_string(char *buf, __uint128_t v) {
	char temp[40];
	int i = 0, j = 0;

	if (v == 0) {
		buf[0] = '0';
		buf[1] = '\0';
		return 1;
	}

	while (v > 0) {
		temp[i++] = '0' + (v % 10);
		v /= 10;
	}

	for (; i > 0; j++) {
		buf[j] = temp[--i];
	}
	buf[j] = '\0';
	return j;
}

size_t int128_t_to_string(char *buf, __int128_t v) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
	const __int128_t int128_min = -((__int128_t)1 << 127);
#pragma GCC diagnostic pop
	const __uint128_t int128_min_abs = (__uint128_t)0x8000000000000000ULL
					   << 64;

	int is_negative = v < 0;
	__uint128_t abs_v;

	if (is_negative) {
		*buf = '-';
		buf++;
		abs_v = v == int128_min ? int128_min_abs : (__uint128_t)(-v);
	} else {
		abs_v = (__uint128_t)v;
	}

	size_t len = uint128_t_to_string(buf, abs_v);
	return is_negative ? len + 1 : len;
}

/* Convert a double to a decimal string in buf.
 * Caller must provide a buffer of at least 41 bytes (sign + 17 digits + point +
 * 17 digits + null). Returns the length of the string (excluding null
 * terminator).
 */
size_t double_to_string(char *buf, double v, int max_decimals) {
	char temp[41];
	size_t pos = 0;
	size_t len;
	int i;
	int is_negative;
	unsigned long long int_part;
	double frac_part;
	int int_start;

	/* Handle special cases: NaN, infinity */
	if (v != v) {
		temp[0] = 'n';
		temp[1] = 'a';
		temp[2] = 'n';
		len = 3;
		memcpy(buf, temp, len);
		buf[len] = '\0';
		return len;
	}
	if (v > 1.7976931348623157e308 || v < -1.7976931348623157e308) {
		if (v < 0) {
			buf[pos++] = '-';
		}
		temp[0] = 'i';
		temp[1] = 'n';
		temp[2] = 'f';
		len = 3;
		memcpy(buf + pos, temp, len);
		len += pos;
		buf[len] = '\0';
		return len;
	}

	/* Handle sign */
	is_negative = v < 0;
	if (is_negative) {
		buf[pos++] = '-';
		v = -v;
	}

	/* Handle zero */
	if (v == 0.0) {
		buf[pos++] = '0';
		buf[pos] = '\0';
		return pos;
	}

	/* Clamp max_decimals (0–17 for double precision) */
	if (max_decimals < 0) max_decimals = 0;
	if (max_decimals > 17) max_decimals = 17;

	/* Integer part */
	int_part = (unsigned long long)v;
	frac_part = v - (double)int_part;
	int_start = pos;

	if (int_part == 0) {
		buf[pos++] = '0';
	} else {
		while (int_part > 0) {
			temp[pos++ - int_start] = '0' + (int_part % 10);
			int_part /= 10;
		}
		for (i = 0; i < (int)(pos - int_start); i++) {
			buf[int_start + i] = temp[pos - int_start - 1 - i];
		}
	}

	/* Decimal point and fractional part */
	if (frac_part > 0 && max_decimals > 0) {
		int digits = 0;
		size_t frac_start;
		double rounding;

		buf[pos++] = '.';
		frac_start = pos;
		/* Round to max_decimals */
		rounding = 0.5;
		for (i = 0; i < max_decimals; i++) rounding /= 10.0;
		frac_part += rounding;
		while (frac_part > 0 && digits < max_decimals) {
			int digit;
			frac_part *= 10;
			digit = (int)frac_part;
			buf[pos++] = '0' + digit;
			frac_part -= digit;
			digits++;
		}
		/* Trim trailing zeros */
		while (pos > frac_start && buf[pos - 1] == '0') {
			pos--;
		}
		/* Remove decimal point if no fractional digits remain */
		if (pos == frac_start) {
			pos--;
		}
	}

	buf[pos] = '\0';
	return pos;
}

__uint128_t __udivti3(__uint128_t a, __uint128_t b) {
	int shift;
	__uint128_t quotient, remainder;
	if (b == 0) return 0; /* Avoid division by zero */
	if (a < b) return 0;  /* Early exit if dividend < divisor */

	quotient = 0;
	remainder = a;
	shift = 0;

	/* Normalize divisor: shift left until highest bit is 1 */
	while (b < ((__uint128_t)1 << 127)) {
		b <<= 1;
		shift++;
	}

	/* Long division */
	while (shift >= 0) {
		if (remainder >= b) {
			remainder -= b;
			quotient |= (__uint128_t)1 << shift;
		}
		b >>= 1;
		shift--;
	}

	return quotient;
}

int open_create(const char *path) {
	return open(path, O_CREAT | O_RDWR, MODE_0644);
}
