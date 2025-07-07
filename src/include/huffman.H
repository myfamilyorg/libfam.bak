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

#ifndef _HUFFMAN_H
#define _HUFFMAN_H

#include <types.H>

#define MAX_HUFFMAN_SYMBOLS 256

typedef struct {
	u8 lengths[MAX_HUFFMAN_SYMBOLS];
	u32 codes[MAX_HUFFMAN_SYMBOLS];
} HuffmanLookup;

const u8 *huffman_lookup_compress(const HuffmanLookup *lookup, u16 *size);
i32 huffmean_lookup_decompress(HuffmanLookup *lookup, const u8 *compressed,
			       u16 size);
i32 huffman_gen(HuffmanLookup *lookup, const u8 *input, u16 len);
i32 huffman_encode(HuffmanLookup *lookup, const u8 *input, u16 len, u8 *output,
		   u32 output_capacity);
i32 huffman_decode(HuffmanLookup *lookup, const u8 *input, u32 len, u8 *output,
		   u32 output_capacity);

#endif /* _HUFFMAN_H */
