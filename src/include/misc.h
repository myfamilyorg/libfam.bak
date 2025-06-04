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

#include <types.h>

size_t strlen(const char *S);
char *strcpy(char *dest, const char *src);
char *strcat(char *dest, const char *src);
int strcmp(const char *s1, const char *s2);
int strcmpn(const char *s1, const char *s2, size_t n);
char *strstr(const char *X, const char *Y);
void *memset(void *ptr, int x, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void bzero(void *dst, size_t n);
size_t uint128_t_to_string(char *buf, uint128_t v);
size_t int128_t_to_string(char *buf, int128_t v);
size_t double_to_string(char *buf, double v, int max_decimals);
int b64_encode(const void *buf_in, size_t in_len, char *buf_out);
int b64_decode(const void *buf_in, size_t in_len, char *buf_out);
uint128_t string_to_uint128(const char *buf, size_t len);
int128_t string_to_int128(const char *buf, size_t len);
void panic(const char *msg);

#endif /* _MISC_H */
