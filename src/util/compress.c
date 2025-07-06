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

#include <compress.H>
#include <error.H>
#include <format.H>

#define LZX_HASH_ENTRIES 4096
#define HASH_CONSTANT 2654435761U
#define MATCH_SENTINEL 0xFD

typedef struct {
	u8 table[LZX_HASH_ENTRIES * 2] __attribute__((aligned(16)));
} LzxHash;

STATIC void lzx_hash_init(LzxHash *hash) {
	u16 i;
	for (i = 0; i < LZX_HASH_ENTRIES * 2; i += 2) {
		*(u16 *)(hash->table + i) = U16_MAX;
	}
}

STATIC u16 lzx_hash_get(LzxHash *hash, u32 key) {
	u32 h = (key * HASH_CONSTANT) >> 20;
	h &= (LZX_HASH_ENTRIES - 1);
	return *(u16 *)(hash->table + h * 2);
}

STATIC void lzx_hash_set(LzxHash *hash, u32 key, u16 value) {
	u32 h = (key * HASH_CONSTANT) >> 20;
	h &= (LZX_HASH_ENTRIES - 1);
	*(u16 *)(hash->table + h * 2) = value;
}

STATIC i32 lzx_escape_only(const u8 *input, u16 in_len, u8 *output,
			   u64 out_capacity) {
	u32 itt = 0, i;
	if (!input || !output || !in_len || out_capacity < 2 * in_len) {
		err = in_len ? ENOBUFS : EINVAL;
		return -1;
	}
	for (i = 0; i < in_len; i++) {
		output[itt++] = input[i];
		if (input[i] == MATCH_SENTINEL) output[itt++] = 0x0;
	}
	return itt;
}

i32 lzx_compress_block(const u8 *input, u16 in_len, u8 *output,
		       u64 out_capacity) {
	LzxHash hash;
	u32 itt = 0, i;
	if (!input || !output || !in_len || !out_capacity) {
		err = EINVAL;
		return -1;
	}

	lzx_hash_init(&hash);

	for (i = 0; i < in_len; i++) {
		if (i + sizeof(u32) <= in_len) {
			u32 key = *(u32 *)(input + i);
			u16 value = lzx_hash_get(&hash, key);
			if (value == U16_MAX || value >= itt) {
				if (itt + (input[i] == MATCH_SENTINEL ? 2 : 1) >
				    out_capacity) {
					err = ENOBUFS;
					return -1;
				}
				output[itt++] = input[i];
				if (input[i] == MATCH_SENTINEL)
					output[itt++] = 0x0;
				if (itt >= 4) {
					key = *(u32 *)(output + (itt - 4));
					lzx_hash_set(&hash, key, itt - 4);
				}
			} else {
				u32 j = 0;
				while (j < 255 && j + i < in_len &&
				       value + j < itt &&
				       input[i + j] == output[value + j])
					j++;

				if (j < 4) {
					if (itt + (input[i] == MATCH_SENTINEL
						       ? 2
						       : 1) >
					    out_capacity) {
						err = ENOBUFS;
						return -1;
					}
					output[itt++] = input[i];
					if (input[i] == MATCH_SENTINEL)
						output[itt++] = 0x0;
				} else {
					if (itt + 4 >= out_capacity) {
						err = ENOBUFS;
						return -1;
					}
					if (itt + 4 > U16_MAX)
						return lzx_escape_only(
						    input, in_len, output,
						    out_capacity);

					output[itt++] = MATCH_SENTINEL;
					output[itt++] = j;
					output[itt++] = value & 0xFF;
					output[itt++] = value >> 8;
					i += j - 1;
				}
			}
		} else {
			if (itt + (input[i] == MATCH_SENTINEL ? 2 : 1) >
			    out_capacity) {
				err = ENOBUFS;
				return -1;
			}
			output[itt++] = input[i];
			if (input[i] == MATCH_SENTINEL) output[itt++] = 0x0;
		}
	}

	return itt;
}

