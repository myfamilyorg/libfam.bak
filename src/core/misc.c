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
#include <limits.H>
#include <misc.H>
#include <sys.H>
#include <types.H>

uint64_t strlen(const char *X) {
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

int strcmpn(const char *X, const char *Y, uint64_t n) {
	while (n > 0 && *X == *Y && *X != '\0' && *Y != '\0') {
		X++;
		Y++;
		n--;
	}
	if (n == 0) return 0;
	return (uint8_t)*X - (uint8_t)*Y;
}

char *substr(const char *s, const char *sub) {
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

char *substrn(const char *s, const char *sub, uint64_t n) {
	uint64_t i;
	if (s == NULL || sub == NULL || n == 0) return NULL;
	if (*sub == '\0') return (char *)s;

	for (i = 0; i < n && s[i]; i++) {
		const char *tmps = s + i, *tmpsub = sub;
		uint64_t j = i;
		while (j < n && *tmps == *tmpsub && *tmps) {
			tmps++;
			tmpsub++;
			j++;
		}
		if (*tmpsub == '\0') return (char *)(s + i);
	}
	return NULL;
}

char *strchr(const char *s, int c) {
	while (*s) {
		if (*s == c) return (char *)s;
		s++;
	}
	return (*s == c) ? (char *)s : 0;
}

void *memset(void *dest, int c, uint64_t n) {
	uint8_t *s = (uint8_t *)dest;
	uint64_t i;

	if (dest == NULL || n == 0) {
		return dest;
	}

	for (i = 0; i < n; i++) {
		s[i] = (uint8_t)c;
	}
	return dest;
}

int memcmp(const void *s1, const void *s2, uint64_t n) {
	/* Cast pointers to unsigned char for byte-by-byte comparison */
	const unsigned char *p1 = (const unsigned char *)s1;
	const unsigned char *p2 = (const unsigned char *)s2;
	uint64_t i;

	/* Compare each byte */
	for (i = 0; i < n; i++) {
		if (p1[i] != p2[i]) {
			/* Return difference as unsigned char values */
			return (int)(p1[i] - p2[i]);
		}
	}

	/* No differences found */
	return 0;
}

void *memcpy(void *dest, const void *src, uint64_t n) {
	uint8_t *d = (uint8_t *)dest;
	const uint8_t *s = (uint8_t *)src;
	uint64_t i;

	if (dest == NULL || src == NULL || n == 0) {
		return dest;
	}

	for (i = 0; i < n; i++) {
		d[i] = s[i];
	}
	return dest;
}

void *memorymove(void *dest, const void *src, uint64_t n) {
	uint8_t *d = (uint8_t *)dest;
	const uint8_t *s = (uint8_t *)src;
	uint64_t i;

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

void byteszero(void *s, uint64_t len) { memset(s, 0, len); }

uint64_t uint128_t_to_string(char *buf, uint128_t v) {
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

uint64_t int128_t_to_string(char *buf, int128_t v) {
	uint64_t len;
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
uint128_t string_to_uint128(const char *buf, uint64_t len) {
	uint128_t result;
	uint64_t i;
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
int128_t string_to_int128(const char *buf, uint64_t len) {
	uint64_t i;
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
	err = 0;
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

uint64_t double_to_string(char *buf, double v, int max_decimals) {
	char temp[41];
	uint64_t pos = 0;
	uint64_t len;
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

	/* Check for overflow in integer part */
	if (v >= 18446744073709551616.0) { /* 2^64 */
		if (is_negative) {
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

	/* Clamp max_decimals (0–17 for double precision) */
	if (max_decimals < 0) max_decimals = 0;
	if (max_decimals > 17) max_decimals = 17;

	/* Integer part */
	int_part = (uint64_t)v;
	frac_part = v - (double)int_part;
	int_start = pos;

	/* Round to max_decimals and handle carry-over */
	if (max_decimals > 0) {
		double rounding = 0.5;
		for (i = 0; i < max_decimals; i++) rounding /= 10.0;
		v += rounding;
		int_part = (uint64_t)v;
		frac_part = v - (double)int_part;
	}

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
		uint64_t frac_start;

		buf[pos++] = '.';
		frac_start = pos;
		while (digits < max_decimals) { /* Stop at max_decimals */
			int digit;
			frac_part *= 10;
			digit = (int)frac_part;
			if (digit > 9) digit = 9; /* Cap digit */
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

uint128_t __umodti3(uint128_t a, uint128_t b) {
	uint64_t b_lo;
	uint64_t a_hi;
	uint64_t a_lo;
	uint128_t remainder;
	int shift;

	/* Handle division by zero */
	if (b == 0) {
		__builtin_trap();
	}

	/* Early return if a < b */
	if (a < b) {
		return a;
	}

	/* If b fits in 64 bits, optimize */
	if ((b >> 64) == 0) {
		b_lo = (uint64_t)b;
		if (b_lo == 0) {
			__builtin_trap();
		}
		a_hi = (uint64_t)(a >> 64);
		a_lo = (uint64_t)a;

		if (a_hi == 0) {
			return (uint128_t)(a_lo % b_lo);
		}

		/* Compute remainder for a_hi != 0 */
		remainder = a_hi % b_lo;
		remainder = (remainder << 32) | (a_lo >> 32);
		remainder = remainder % b_lo;
		remainder = (remainder << 32) | (a_lo & 0xffffffff);
		remainder = remainder % b_lo;
		return remainder;
	}

	/* General 128-bit case */
	remainder = a;

	/* Align b with remainder's MSB */
	shift = 0;
	while ((b << shift) <= remainder && shift < 128) {
		shift++;
	}
	if (shift > 0) {
		shift--;
		b <<= shift;
	}

	/* Division loop */
	while (shift >= 0) {
		if (remainder >= b) {
			remainder -= b;
		}
		b >>= 1;
		shift--;
	}

	return remainder;
}

uint128_t __udivti3(uint128_t a, uint128_t b) {
	uint64_t b_lo;
	uint64_t a_hi;
	uint64_t a_lo;
	uint128_t quotient;
	uint128_t remainder;
	int shift;

	/* Handle division by zero */
	if (b == 0) {
		__builtin_trap();
	}

	/* Early return if a < b */
	if (a < b) {
		return 0;
	}

	/* If b fits in 64 bits, optimize */
	if ((b >> 64) == 0) {
		b_lo = (uint64_t)b;
		if (b_lo == 0) {
			__builtin_trap();
		}
		a_hi = (uint64_t)(a >> 64);
		a_lo = (uint64_t)a;

		if (a_hi == 0) {
			return (uint128_t)(a_lo / b_lo);
		}

		/* Compute quotient for a_hi != 0 */
		quotient = (uint128_t)a_hi / b_lo;
		quotient <<= 32;
		quotient |= (uint128_t)(a_lo >> 32) / b_lo;
		quotient <<= 32;
		quotient |= (uint128_t)(a_lo & 0xffffffff) / b_lo;
		return quotient;
	}

	/* General 128-bit case */
	quotient = 0;
	remainder = a;

	/* Align b with remainder's MSB */
	shift = 0;
	while ((b << shift) <= remainder && shift < 128) {
		shift++;
	}
	if (shift > 0) {
		shift--; /* Adjust to highest valid shift */
		b <<= shift;
	}

	/* Division loop */
	while (shift >= 0) {
		if (remainder >= b) {
			remainder -= b;
			quotient |= ((uint128_t)1) << shift;
		}
		b >>= 1;
		shift--;
	}

	return quotient;
}
int printf(const char *, ...);
/* Base64 encode */
uint64_t b64_encode(const uint8_t *in, uint64_t in_len, uint8_t *out,
		  uint64_t out_max) {
	uint64_t i;
	uint64_t j;
	static const char *b64_table =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	if (!in || !out || out_max < ((in_len + 2) / 3) * 4 + 1) {
		return 0;
	}

	j = 0;
	for (i = 0; i + 2 < in_len; i += 3) {
		out[j] = b64_table[(in[i] >> 2) & 0x3F];
		j++;
		out[j] = b64_table[((in[i] & 0x3) << 4) | (in[i + 1] >> 4)];
		j++;
		out[j] = b64_table[((in[i + 1] & 0xF) << 2) | (in[i + 2] >> 6)];
		j++;
		out[j] = b64_table[in[i + 2] & 0x3F];
		j++;
	}

	if (i < in_len) {
		out[j] = b64_table[(in[i] >> 2) & 0x3F];
		j++;
		if (i + 1 < in_len) {
			out[j] =
			    b64_table[((in[i] & 0x3) << 4) | (in[i + 1] >> 4)];
			j++;
			out[j] = b64_table[(in[i + 1] & 0xF) << 2];
			j++;
		} else {
			out[j] = b64_table[(in[i] & 0x3) << 4];
			j++;
			out[j] = '=';
			j++;
		}
		out[j] = '=';
		j++;
	}

	out[j] = '\0';
	return j;
}

/* Base64 decode */
uint64_t b64_decode(const uint8_t *in, uint64_t in_len, uint8_t *out,
		  uint64_t out_max) {
	uint64_t i;
	uint64_t j;
	int b0;
	int b1;
	int b2;
	int b3;
	static const int decode_table[256] = {
	    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	    -1, 0,  1,	2,  3,	4,  5,	6,  7,	8,  9,	10, 11, 12, 13, 14,
	    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

	if (!in || !out || in_len % 4 != 0) {
		return 0;
	}

	/* Account for padding in output size */
	{
		uint64_t expected_out = (in_len / 4) * 3;
		if (in_len >= 4 && in[in_len - 1] == '=') out_max++;
		if (in_len >= 4 && in[in_len - 2] == '=') out_max++;
		if (out_max < expected_out) {
			return 0;
		}
	}

	j = 0;
	for (i = 0; i < in_len; i += 4) {
		b0 = decode_table[(unsigned char)in[i]];
		b1 = decode_table[(unsigned char)in[i + 1]];
		b2 = (i + 2 < in_len && in[i + 2] != '=')
			 ? decode_table[(unsigned char)in[i + 2]]
			 : -1;
		b3 = (i + 3 < in_len && in[i + 3] != '=')
			 ? decode_table[(unsigned char)in[i + 3]]
			 : -1;

		if (b0 == -1 || b1 == -1 ||
		    (i + 2 < in_len && in[i + 2] != '=' && b2 == -1) ||
		    (i + 3 < in_len && in[i + 3] != '=' && b3 == -1)) {
			return 0;
		}

		if (j + 3 > out_max) {
			return 0;
		}
		out[j] = (unsigned char)((b0 << 2) | (b1 >> 4));
		j++;
		if (i + 2 < in_len && in[i + 2] != '=') {
			out[j] = (unsigned char)((b1 << 4) | (b2 >> 2));
			j++;
		}
		if (i + 3 < in_len && in[i + 3] != '=') {
			out[j] = (unsigned char)((b2 << 6) | b3);
			j++;
		}
	}

	return j;
}
