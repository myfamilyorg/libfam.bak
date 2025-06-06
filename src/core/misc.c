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

#include <error.h>
#include <limits.h>
#include <misc.h>
#include <sys.h>
#include <types.h>

void panic(const char *msg) {
	write(2, msg, strlen(msg));
	write(2, "\n", 1);
	exit(-1);
}

size_t strlen(const char *X) {
	const char *Y;
	if (X == NULL) return 0;
	Y = X;
	while (*X) X++;
	return X - Y;
}

char *strcpy(char *dest, const char *src) {
	char *ptr = dest;
	while ((*ptr++ = *src++));
	return dest;
}

char *strcat(char *dest, const char *src) {
	char *ptr = dest;
	while (*ptr) ptr++;
	while ((*ptr++ = *src++));
	return dest;
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
	if ((uint8_t)*X > (uint8_t)*Y) return 1;
	if ((uint8_t)*Y > (uint8_t)*X) return -1;
	return 0;
}

int strcmpn(const char *X, const char *Y, size_t n) {
	while (n > 0 && *X == *Y && *X != '\0' && *Y != '\0') {
		X++;
		Y++;
		n--;
	}
	if (n == 0) return 0;
	return (uint8_t)*X - (uint8_t)*Y;
}

char *strstr(const char *s, const char *sub) {
	if (s == NULL || sub == NULL) return NULL;
	for (; *s; s++) {
		const char *tmps = s, *tmpsub = sub;
		while (*(uint8_t *)tmps == *(uint8_t *)tmpsub && *tmps) {
			tmps++;
			tmpsub++;
		}
		if (*tmpsub == '\0') return (char *)s;
	}
	return NULL;
}

void *memset(void *dest, int c, size_t n) {
	uint8_t *s = (uint8_t *)dest;
	size_t i;

	if (dest == NULL || n == 0) {
		return dest;
	}

	for (i = 0; i < n; i++) {
		s[i] = (uint8_t)c;
	}
	return dest;
}

void *memcpy(void *dest, const void *src, size_t n) {
	uint8_t *d = (uint8_t *)dest;
	const uint8_t *s = (uint8_t *)src;
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
	uint8_t *d = (uint8_t *)dest;
	const uint8_t *s = (uint8_t *)src;
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

size_t uint128_t_to_string(char *buf, uint128_t v) {
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

size_t int128_t_to_string(char *buf, int128_t v) {
	size_t len;
	const int128_t int128_min = INT128_MIN;
	const uint128_t int128_min_abs = (uint128_t)0x8000000000000000UL << 64;

	int is_negative = v < 0;
	uint128_t abs_v;

	if (is_negative) {
		*buf = '-';
		buf++;
		abs_v = v == int128_min ? int128_min_abs : (uint128_t)(-v);
	} else {
		abs_v = (uint128_t)v;
	}

	len = uint128_t_to_string(buf, abs_v);
	return is_negative ? len + 1 : len;
}

/* Convert string to unsigned 128-bit integer */
uint128_t string_to_uint128(const char *buf, size_t len) {
	uint128_t result;
	size_t i;
	char c;

	/* Input validation */
	if (!buf || len == 0) {
		err = EINVAL;
		return (uint128_t)0;
	}

	result = (uint128_t)0;
	i = 0;

	/* Skip leading whitespace */
	while (i < len && (buf[i] == ' ' || buf[i] == '\t')) {
		i++;
	}

	if (i == len) {
		err = EINVAL;
		return (uint128_t)0;
	}

	/* Process digits */
	while (i < len) {
		c = buf[i];
		if (c < '0' || c > '9') {
			err = EINVAL;
			return (uint128_t)0;
		}

		/* Check for overflow */
		if (result > UINT128_MAX / 10) {
			err = EINVAL;
			return (uint128_t)0;
		}

		result = result * 10 + (c - '0');
		i++;
	}

	return result;
}

/* Convert string to signed 128-bit integer using string_to_uint128 */
int128_t string_to_int128(const char *buf, size_t len) {
	size_t i;
	int sign;
	uint128_t abs_value;
	int128_t result;

	/* Input validation */
	if (!buf || len == 0) {
		err = EINVAL;
		return (int128_t)0;
	}

	i = 0;
	sign = 1;

	/* Skip leading whitespace */
	while (i < len && (buf[i] == ' ' || buf[i] == '\t')) {
		i++;
	}

	if (i == len) {
		err = EINVAL;
		return (int128_t)0;
	}

	/* Handle sign */
	if (buf[i] == '-') {
		sign = -1;
		i++;
	} else if (buf[i] == '+') {
		i++;
	}

	if (i == len) {
		err = EINVAL;
		return (int128_t)0;
	}

	/* Use string_to_uint128 for absolute value */
	abs_value = string_to_uint128(buf + i, len - i);
	if (err == EINVAL) {
		return (int128_t)0; /* Propagate error */
	}

	/* Check for overflow */
	if (abs_value > (uint128_t)INT128_MAX + (sign == -1 ? 1 : 0)) {
		err = EINVAL;
		return (int128_t)0;
	}

	result = sign * (int128_t)abs_value;
	return result;
}

/* Convert a double to a decimal string in buf.
 * Caller must provide a buffer of at least 41 uint8_ts (sign + 17 digits +
 * point + 17 digits + null). Returns the length of the string (excluding null
 * terminator).
 */
size_t double_to_string(char *buf, double v, int max_decimals) {
	char temp[41];
	size_t pos = 0;
	size_t len;
	int i;
	int is_negative;
	uint64_t int_part;
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
	int_part = (uint64_t)v;
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

uint128_t __udivti3(uint128_t a, uint128_t b) {
	int shift;
	uint128_t quotient, remainder;
	if (b == 0) return 0; /* Avoid division by zero */
	if (a < b) return 0;  /* Early exit if dividend < divisor */

	quotient = 0;
	remainder = a;
	shift = 0;

	/* Normalize divisor: shift left until highest bit is 1 */
	while (b < ((uint128_t)1 << 127)) {
		b <<= 1;
		shift++;
	}

	/* Long division */
	while (shift >= 0) {
		if (remainder >= b) {
			remainder -= b;
			quotient |= (uint128_t)1 << shift;
		}
		b >>= 1;
		shift--;
	}

	return quotient;
}

uint128_t __umodti3(uint128_t a, uint128_t b) {
	uint128_t quotient;
	uint128_t remainder;
	int shift, i;
	/* Handle division by zero */
	if (b == 0) {
		__builtin_trap(); /* Trigger undefined behavior or abort */
	}

	/* If b is a 64-bit value, use simpler division */
	if ((b >> 64) == 0) {
		uint64_t b_lo = (uint64_t)b, a_hi, a_lo, rem;
		if (b_lo == 0) {
			__builtin_trap();
		}
		/* Extract high and low 64 bits of a */
		a_hi = (uint64_t)(a >> 64);
		a_lo = (uint64_t)a;

		/* If a_hi < b_lo, remainder is a_lo % b_lo */
		if (a_hi == 0) {
			return (uint128_t)(a_lo % b_lo);
		}

		/* Perform long division */
		rem = a_hi;
		rem = (rem << 32) | (a_lo >> 32);
		rem = rem % b_lo;
		rem = (rem << 32) | (a_lo & 0xffffffff);
		return (uint128_t)(rem % b_lo);
	}

	/* General case: 128-bit division */
	quotient = 0;
	remainder = a;

	/* Normalize: shift b until its highest bit is 1 */
	shift = __builtin_clzll((uint64_t)(b >> 64));
	if (shift == 64) {
		shift += __builtin_clzll((uint64_t)b);
	}
	b <<= shift;
	remainder <<= shift;

	/* Long division loop */
	for (i = 0; i <= shift; i++) {
		if (remainder >= b) {
			remainder -= b;
			quotient |= (uint128_t)1 << (shift - i);
		}
		b >>= 1;
	}

	return remainder;
}

int b64_encode(const void *buf_in, size_t in_len, char *buf_out) {
	/* Base64 alphabet */
	static const char *b64_table =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	const unsigned char *in = (const unsigned char *)buf_in;
	size_t i, out_len = 0;

	/* Ensure output buffer is not null */
	if (!buf_out) return -1;

	/* Process input in groups of 3 uint8_ts */
	for (i = 0; i < in_len; i += 3) {
		/* First 6 bits of first uint8_t */
		buf_out[out_len++] = b64_table[(in[i] >> 2) & 0x3F];

		/* Last 2 bits of first uint8_t + first 4 bits of second uint8_t
		 */
		if (i + 1 < in_len) {
			buf_out[out_len++] =
			    b64_table[((in[i] & 0x3) << 4) |
				      ((in[i + 1] >> 4) & 0xF)];
		} else {
			buf_out[out_len++] = b64_table[(in[i] & 0x3) << 4];
			buf_out[out_len++] = '=';
			buf_out[out_len++] = '=';
			break;
		}

		/* Last 4 bits of second uint8_t + first 2 bits of third uint8_t
		 */
		if (i + 2 < in_len) {
			buf_out[out_len++] =
			    b64_table[((in[i + 1] & 0xF) << 2) |
				      ((in[i + 2] >> 6) & 0x3)];
			buf_out[out_len++] = b64_table[in[i + 2] & 0x3F];
		} else {
			buf_out[out_len++] = b64_table[(in[i + 1] & 0xF) << 2];
			buf_out[out_len++] = '=';
		}
	}

	buf_out[out_len] = '\0'; /* Null-terminate output */
	return out_len;
}

int b64_decode(const void *buf_in, size_t in_len, char *buf_out) {
	/* Reverse lookup table for Base64 characters */
	static const unsigned char b64_lookup[] = {
	    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	    255, 255, 255, 255, 255, 255, 62,  255, 255, 52,  53,  54,	55,
	    56,	 57,  58,  59,	60,  61,  255, 255, 255, 64,  255, 255, 255,
	    0,	 1,   2,   3,	4,   5,	  6,   7,   8,	 9,   10,  11,	12,
	    13,	 14,  15,  16,	17,  18,  19,  20,  21,	 22,  23,  24,	25,
	    255, 255, 255, 255, 63,  255, 26,  27,  28,	 29,  30,  31,	32,
	    33,	 34,  35,  36,	37,  38,  39,  40,  41,	 42,  43,  44,	45,
	    46,	 47,  48,  49,	50,  51,  255, 255, 255, 255, 255};
	const unsigned char *in = (const unsigned char *)buf_in;
	size_t i, out_len = 0;

	/* Ensure input length is valid (multiple of 4) and output buffer is not
	 */
	/* null */
	if (in_len % 4 != 0 || !buf_out) return -1;

	/* Process input in groups of 4 uint8_ts */
	for (i = 0; i < in_len; i += 4) {
		unsigned char c1, c2, c3, c4;

		/* Validate input characters */
		if (in[i] > 127 || in[i + 1] > 127 || in[i + 2] > 127 ||
		    in[i + 3] > 127)
			return -1;
		c1 = b64_lookup[in[i]];
		c2 = b64_lookup[in[i + 1]];
		c3 = b64_lookup[in[i + 2]];
		c4 = b64_lookup[in[i + 3]];
		if (c1 == 255 || c2 == 255 || (c3 == 255 && in[i + 2] != '=') ||
		    (c4 == 255 && in[i + 3] != '='))
			return -1;

		/* First output uint8_t */
		buf_out[out_len++] = (c1 << 2) | ((c2 >> 4) & 0x3);

		/* Second output uint8_t (if not padding) */
		if (in[i + 2] != '=') {
			buf_out[out_len++] =
			    ((c2 & 0xF) << 4) | ((c3 >> 2) & 0xF);
		}

		/* Third output uint8_t (if not padding) */
		if (in[i + 3] != '=') {
			buf_out[out_len++] = ((c3 & 0x3) << 6) | c4;
		}
	}

	buf_out[out_len] = '\0'; /* Null-terminate output */
	return out_len;
}
