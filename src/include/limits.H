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

#ifndef _LIMITS_H
#define _LIMITS_H

#ifndef U8_MIN
#define U8_MIN 0x0
#endif

#ifndef U16_MIN
#define U16_MIN 0x0
#endif

#ifndef U32_MIN
#define U32_MIN 0x0
#endif

#ifndef U64_MIN
#define U64_MIN 0x0
#endif

#ifndef U128_MIN
#define U128_MIN 0x0
#endif

#ifndef U8_MAX
#define U8_MAX 0xFF
#endif

#ifndef U16_MAX
#define U16_MAX 0xFFFF
#endif

#ifndef U32_MAX
#define U32_MAX 0xFFFFFFFF
#endif

#ifndef U64_MAX
#define U64_MAX 0xFFFFFFFFFFFFFFFF
#endif

#ifndef U128_MAX
#define U128_MAX \
	(((u128)0xFFFFFFFFFFFFFFFFUL << 64) | 0xFFFFFFFFFFFFFFFFUL)
#endif

#ifndef I8_MIN
#define I8_MIN (-0x7F - 1)
#endif

#ifndef I16_MIN
#define I16_MIN (-0x7FFF - 1)
#endif

#ifndef I32_MIN
#define I32_MIN (-0x7FFFFFFF - 1)
#endif

#ifndef I64_MIN
#define I64_MIN (-0x7FFFFFFFFFFFFFFF - 1)
#endif

#define I128_MIN (((i128)0x80000000UL << 96))

#ifndef I8_MAX
#define I8_MAX 0x7F
#endif

#ifndef I16_MAX
#define I16_MAX 0x7FFF
#endif

#ifndef I32_MAX
#define I32_MAX 0x7FFFFFFF
#endif

#ifndef I64_MAX
#define I64_MAX 0x7FFFFFFFFFFFFFFF
#endif

#ifndef I128_MAX
#define I128_MAX                                                         \
	(((i128)0x7FFFFFFFUL << 96) | ((i128)0xFFFFFFFFUL << 64) | \
	 0xFFFFFFFFUL)
#endif

#ifndef I_MAX
#define I_MAX __I_MAX__
#endif

#ifndef SIZE_MAX
#define SIZE_MAX U64_MAX
#endif

#endif /* _LIMITS_H */

