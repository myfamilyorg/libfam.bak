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

u64 strlen(const u8 *X) {
	const u8 *Y;
	if (X == NULL) return 0;
	Y = X;
	while (*X) X++;
	return X - Y;
}

u8 *strcpy(u8 *dest, const u8 *src) {
	u8 *ptr = dest;
	while ((*ptr++ = *src++));
	return dest;
}

u8 *strcat(u8 *dest, const u8 *src) {
	u8 *ptr = dest;
	while (*ptr) ptr++;
	while ((*ptr++ = *src++));
	return dest;
}

i32 strcmp(const u8 *X, const u8 *Y) {
	if (X == NULL || Y == NULL) {
		if (X == Y) return 0;
		return X == NULL ? -1 : 1;
	}
	while (*X == *Y && *X) {
		X++;
		Y++;
	}
	if ((u8)*X > (u8)*Y) return 1;
	if ((u8)*Y > (u8)*X) return -1;
	return 0;
}

i32 strcmpn(const u8 *X, const u8 *Y, u64 n) {
	while (n > 0 && *X == *Y && *X != '\0' && *Y != '\0') {
		X++;
		Y++;
		n--;
	}
	if (n == 0) return 0;
	return (u8)*X - (u8)*Y;
}

u8 *substr(const u8 *s, const u8 *sub) {
	if (s == NULL || sub == NULL) return NULL;
	for (; *s; s++) {
		const u8 *tmps = s, *tmpsub = sub;
		while (*(u8 *)tmps == *(u8 *)tmpsub && *tmps) {
			tmps++;
			tmpsub++;
		}
		if (*tmpsub == '\0') return (u8 *)s;
	}
	return NULL;
}

u8 *substrn(const u8 *s, const u8 *sub, u64 n) {
	u64 i;
	if (s == NULL || sub == NULL || n == 0) return NULL;
	if (*sub == '\0') return (u8 *)s;

	for (i = 0; i < n && s[i]; i++) {
		const u8 *tmps = s + i, *tmpsub = sub;
		u64 j = i;
		while (j < n && *tmps == *tmpsub && *tmps) {
			tmps++;
			tmpsub++;
			j++;
		}
		if (*tmpsub == '\0') return (u8 *)(s + i);
	}
	return NULL;
}

u8 *strchr(const u8 *s, i32 c) {
	while (*s) {
		if (*s == c) return (u8 *)s;
		s++;
	}
	return (*s == c) ? (u8 *)s : 0;
}

void *memset(void *dest, i32 c, u64 n) {
	u8 *s = (u8 *)dest;
	u64 i;

	if (dest == NULL || n == 0) {
		return dest;
	}

	for (i = 0; i < n; i++) {
		s[i] = (u8)c;
	}
	return dest;
}

i32 memcmp(const void *s1, const void *s2, u64 n) {
	/* Cast pointers to u8 for byte-by-byte comparison */
	const u8 *p1 = (const u8 *)s1;
	const u8 *p2 = (const u8 *)s2;
	u64 i;

	/* Compare each byte */
	for (i = 0; i < n; i++) {
		if (p1[i] != p2[i]) {
			/* Return difference as u8 values */
			return (i32)(p1[i] - p2[i]);
		}
	}

	/* No differences found */
	return 0;
}

void *memcpy(void *dest, const void *src, u64 n) {
	u8 *d = (u8 *)dest;
	const u8 *s = (u8 *)src;
	u64 i;

	if (dest == NULL || src == NULL || n == 0) {
		return dest;
	}

	for (i = 0; i < n; i++) {
		d[i] = s[i];
	}
	return dest;
}

void *memorymove(void *dest, const void *src, u64 n) {
	u8 *d = (u8 *)dest;
	const u8 *s = (u8 *)src;
	u64 i;

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

void byteszero(void *s, u64 len) { memset(s, 0, len); }

u64 u128_to_string(u8 *buf, u128 v) {
	u8 temp[40];
	i32 i = 0, j = 0;

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

u64 i128_to_string(u8 *buf, i128 v) {
	u64 len;
	const i128 int128_min = INT128_MIN;
	const u128 int128_min_abs = (u128)0x8000000000000000UL << 64;

	i32 is_negative = v < 0;
	u128 abs_v;

	if (is_negative) {
		*buf = '-';
		buf++;
		abs_v = v == int128_min ? int128_min_abs : (u128)(-v);
	} else {
		abs_v = (u128)v;
	}

	len = u128_to_string(buf, abs_v);
	return is_negative ? len + 1 : len;
}

/* Convert string to unsigned 128-bit integer */
u128 string_to_uint128(const u8 *buf, u64 len) {
	u128 result;
	u64 i;
	u8 c;

	/* Input validation */
	if (!buf || len == 0) {
		err = EINVAL;
		return (u128)0;
	}

	result = (u128)0;
	i = 0;

	/* Skip leading whitespace */
	while (i < len && (buf[i] == ' ' || buf[i] == '\t')) {
		i++;
	}

	if (i == len) {
		err = EINVAL;
		return (u128)0;
	}

	/* Process digits */
	while (i < len) {
		c = buf[i];
		if (c < '0' || c > '9') {
			err = EINVAL;
			return (u128)0;
		}

		/* Check for overflow */
		if (result > UINT128_MAX / 10) {
			err = EINVAL;
			return (u128)0;
		}

		result = result * 10 + (c - '0');
		i++;
	}

	return result;
}

/* Convert string to signed 128-bit integer using string_to_uint128 */
i128 string_to_int128(const u8 *buf, u64 len) {
	u64 i;
	i32 sign;
	u128 abs_value;
	i128 result;

	/* Input validation */
	if (!buf || len == 0) {
		err = EINVAL;
		return (i128)0;
	}

	i = 0;
	sign = 1;

	/* Skip leading whitespace */
	while (i < len && (buf[i] == ' ' || buf[i] == '\t')) {
		i++;
	}

	if (i == len) {
		err = EINVAL;
		return (i128)0;
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
		return (i128)0;
	}

	/* Use string_to_uint128 for absolute value */
	err = 0;
	abs_value = string_to_uint128(buf + i, len - i);
	if (err == EINVAL) {
		return (i128)0; /* Propagate error */
	}

	/* Check for overflow */
	if (abs_value > (u128)INT128_MAX + (sign == -1 ? 1 : 0)) {
		err = EINVAL;
		return (i128)0;
	}

	result = sign * (i128)abs_value;
	return result;
}

u64 double_to_string(u8 *buf, double v, i32 max_decimals) {
	u8 temp[41];
	u64 pos = 0;
	u64 len;
	i32 i;
	i32 is_negative;
	u64 int_part;
	double frac_part;
	i32 int_start;

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
	int_part = (u64)v;
	frac_part = v - (double)int_part;
	int_start = pos;

	/* Round to max_decimals and handle carry-over */
	if (max_decimals > 0) {
		double rounding = 0.5;
		for (i = 0; i < max_decimals; i++) rounding /= 10.0;
		v += rounding;
		int_part = (u64)v;
		frac_part = v - (double)int_part;
	}

	if (int_part == 0) {
		buf[pos++] = '0';
	} else {
		while (int_part > 0) {
			temp[pos++ - int_start] = '0' + (int_part % 10);
			int_part /= 10;
		}
		for (i = 0; i < (i32)(pos - int_start); i++) {
			buf[int_start + i] = temp[pos - int_start - 1 - i];
		}
	}

	/* Decimal point and fractional part */
	if (frac_part > 0 && max_decimals > 0) {
		i32 digits = 0;
		u64 frac_start;

		buf[pos++] = '.';
		frac_start = pos;
		while (digits < max_decimals) { /* Stop at max_decimals */
			i32 digit;
			frac_part *= 10;
			digit = (i32)frac_part;
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

u128 __umodti3(u128 a, u128 b) {
	u64 b_lo;
	u64 a_hi;
	u64 a_lo;
	u128 remainder;
	i32 shift;

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
		b_lo = (u64)b;
		if (b_lo == 0) {
			__builtin_trap();
		}
		a_hi = (u64)(a >> 64);
		a_lo = (u64)a;

		if (a_hi == 0) {
			return (u128)(a_lo % b_lo);
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

u128 __udivti3(u128 a, u128 b) {
	u64 b_lo;
	u64 a_hi;
	u64 a_lo;
	u128 quotient;
	u128 remainder;
	i32 shift;

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
		b_lo = (u64)b;
		if (b_lo == 0) {
			__builtin_trap();
		}
		a_hi = (u64)(a >> 64);
		a_lo = (u64)a;

		if (a_hi == 0) {
			return (u128)(a_lo / b_lo);
		}

		/* Compute quotient for a_hi != 0 */
		quotient = (u128)a_hi / b_lo;
		quotient <<= 32;
		quotient |= (u128)(a_lo >> 32) / b_lo;
		quotient <<= 32;
		quotient |= (u128)(a_lo & 0xffffffff) / b_lo;
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
			quotient |= ((u128)1) << shift;
		}
		b >>= 1;
		shift--;
	}

	return quotient;
}

/* Base64 encode */
u64 b64_encode(const u8 *in, u64 in_len, u8 *out, u64 out_max) {
	u64 i;
	u64 j;
	static const u8 *b64_table =
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
u64 b64_decode(const u8 *in, u64 in_len, u8 *out, u64 out_max) {
	u64 i;
	u64 j;
	i32 b0;
	i32 b1;
	i32 b2;
	i32 b3;
	static const i32 decode_table[256] = {
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
		u64 expected_out = (in_len / 4) * 3;
		if (in_len >= 4 && in[in_len - 1] == '=') out_max++;
		if (in_len >= 4 && in[in_len - 2] == '=') out_max++;
		if (out_max < expected_out) {
			return 0;
		}
	}

	j = 0;
	for (i = 0; i < in_len; i += 4) {
		b0 = decode_table[(u8)in[i]];
		b1 = decode_table[(u8)in[i + 1]];
		b2 = (i + 2 < in_len && in[i + 2] != '=')
			 ? decode_table[(u8)in[i + 2]]
			 : -1;
		b3 = (i + 3 < in_len && in[i + 3] != '=')
			 ? decode_table[(u8)in[i + 3]]
			 : -1;

		if (b0 == -1 || b1 == -1 ||
		    (i + 2 < in_len && in[i + 2] != '=' && b2 == -1) ||
		    (i + 3 < in_len && in[i + 3] != '=' && b3 == -1)) {
			return 0;
		}

		if (j + 3 > out_max) {
			return 0;
		}
		out[j] = (u8)((b0 << 2) | (b1 >> 4));
		j++;
		if (i + 2 < in_len && in[i + 2] != '=') {
			out[j] = (u8)((b1 << 4) | (b2 >> 2));
			j++;
		}
		if (i + 3 < in_len && in[i + 3] != '=') {
			out[j] = (u8)((b2 << 6) | b3);
			j++;
		}
	}

	return j;
}
