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

#ifndef _FORMMAT2_H
#define _FORMMAT2_H

#include <macro_util.H>
#include <types.H>

extern bool _debug_no_exit;
extern bool _debug_no_write;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvariadic-macros"

typedef struct {
	u8 *buf;
	u64 capacity;
	u64 pos;
} Formatter;

typedef enum { I128_T, U128_T, STRING_T, DOUBLE_T } PrintableType;

typedef struct {
	PrintableType t;
	union {
		i128 ivalue;
		u128 uvalue;
		u8 *svalue;
		double dvalue;
	} data;
} Printable;

i32 format_append(Formatter *f, const char *fmt, ...);
void format_clear(Formatter *f);
const u8 *format_to_string(Formatter *f);

#define FORMAT_ITEM(arg, value)                                               \
	_Generic((value),                                                     \
	    i128: ((Printable){.t = I128_T, .data.ivalue = (value)}),         \
	    i64: ((Printable){.t = I128_T, .data.ivalue = (i128)(value)}),    \
	    i32: ((Printable){.t = I128_T, .data.ivalue = (i128)(value)}),    \
	    i16: ((Printable){.t = I128_T, .data.ivalue = (i128)(value)}),    \
	    i8: ((Printable){.t = I128_T, .data.ivalue = (i128)(value)}),     \
	    u128: ((Printable){.t = U128_T, .data.uvalue = (value)}),         \
	    u64: ((Printable){.t = U128_T, .data.uvalue = (u128)(value)}),    \
	    u32: ((Printable){.t = U128_T, .data.uvalue = (u128)(value)}),    \
	    u16: ((Printable){.t = U128_T, .data.uvalue = (u128)(value)}),    \
	    u8: ((Printable){.t = U128_T, .data.uvalue = (u128)(value)}),     \
	    u8 *: ((Printable){.t = STRING_T, .data.svalue = (value)}),       \
	    const u8 *: ((Printable){.t = STRING_T,                           \
				     .data.svalue = (u8 *)(value)}),          \
	    char *: ((Printable){.t = STRING_T,                               \
				 .data.svalue = (u8 *)(value)}),              \
	    const char *: ((Printable){.t = STRING_T,                         \
				       .data.svalue = (u8 *)(value)}),        \
	    i8 *: ((Printable){.t = STRING_T, .data.svalue = (u8 *)(value)}), \
	    const i8 *: ((Printable){.t = STRING_T,                           \
				     .data.svalue = (u8 *)(value)}),          \
	    default: ((Printable){.t = STRING_T,                              \
				  .data.svalue = (u8 *)"unsupported"}))

/* clang-format off */
#define format(f, fmt, ...)                                                    \
	_Pragma("GCC diagnostic push");                                        \
	_Pragma("GCC diagnostic ignored \"-Wpragmas\"");                       \
	_Pragma("GCC diagnostic ignored \"-Wunknown-warning-option\"");        \
	_Pragma("GCC diagnostic ignored \"-Wdiscarded-qualifiers\"");          \
	_Pragma("GCC diagnostic ignored \"-Wpointer-to-int-cast\"");           \
	_Pragma("GCC diagnostic ignored \"-Wint-to-pointer-cast\"");           \
        _Pragma("GCC diagnostic ignored \"-Wint-conversion\"");                \
	_Pragma("GCC diagnostic ignored \"-Wincompatible-pointer-types-discards-qualifiers\""); \
	format_append(                                                         \
	    f, fmt __VA_OPT__(, FOR_EACH(FORMAT_ITEM, _, (, ), __VA_ARGS__))); \
	_Pragma("GCC diagnostic pop");
/* clang-format on */

#define RESTORE_DIAGNOSTICS _Pragma("GCC diagnostic pop")

#define println(fmt, ...)                        \
	do {                                     \
		const u8 *ret;                   \
		Formatter _f__ = {0};            \
		format(&_f__, fmt, __VA_ARGS__); \
		ret = format_to_string(&_f__);   \
		write(2, ret, strlen(ret));      \
		write(2, "\n", 1);               \
		format_clear(&_f__);             \
	} while (false);

#define print(fmt, ...)                          \
	do {                                     \
		const u8 *ret;                   \
		Formatter _f__ = {0};            \
		format(&_f__, fmt, __VA_ARGS__); \
		ret = format_to_string(&_f__);   \
		write(2, ret, strlen(ret));      \
		format_clear(&_f__);             \
	} while (false);

#define panic(fmt, ...)                          \
	do {                                     \
		const u8 *ret;                   \
		Formatter _f__ = {0};            \
		format(&_f__, fmt, __VA_ARGS__); \
		ret = format_to_string(&_f__);   \
		write(2, ret, strlen(ret));      \
		write(2, "\n", 1);               \
		format_clear(&_f__);             \
		if (!_debug_no_exit) exit(-1);   \
	} while (false);

#pragma GCC diagnostic pop

#endif /* _FORMMAT2_H */
