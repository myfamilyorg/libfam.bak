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

#ifndef _MISC_H
#define _MISC_H

#include <libfam/types.H>

PUBLIC u64 strlen(const u8 *S);
PUBLIC u8 *strcpy(u8 *dest, const u8 *src);
PUBLIC u8 *strcat(u8 *dest, const u8 *src);
PUBLIC i32 strcmp(const u8 *s1, const u8 *s2);
PUBLIC i32 strcmpn(const u8 *s1, const u8 *s2, u64 n);
PUBLIC u8 *strdup(const u8 *s);
PUBLIC u8 *substr(const u8 *s, const u8 *sub);
PUBLIC u8 *substrn(const u8 *s, const u8 *sub, u64 n);
PUBLIC u8 *strchr(const u8 *s, i32 c);
PUBLIC void *memset(void *ptr, i32 x, u64 n);
PUBLIC void *memcpy(void *dst, const void *src, u64 n);
PUBLIC i32 memcmp(const void *s1, const void *s2, u64 n);
PUBLIC void *memorymove(void *dst, const void *src, u64 n);
PUBLIC void byteszero(void *dst, u64 n);
PUBLIC u64 u128_to_string(u8 *buf, u128 v);
PUBLIC u64 i128_to_string(u8 *buf, i128 v);
PUBLIC u64 u128_to_string_impl(u8 *buf, u128 v, bool hex, bool upper);
PUBLIC u64 i128_to_string_impl(u8 *buf, i128 v, bool hex, bool upper);
PUBLIC u64 double_to_string(u8 *buf, double v, i32 max_decimals);
PUBLIC u64 b64_encode(const u8 *in, u64 in_len, u8 *out, u64 out_max);
PUBLIC u64 b64_decode(const u8 *in, u64 in_len, u8 *out, u64 out_max);
PUBLIC u128 string_to_uint128(const u8 *buf, u64 len);
PUBLIC i128 string_to_int128(const u8 *buf, u64 len);

#endif /* _MISC_H */
