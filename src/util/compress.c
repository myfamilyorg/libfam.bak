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
#define MIN_MATCH 6

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

/*
STATIC u8 lzx_match_len(const u8 *input, u16 in_len, u16 in_pos, u8 *output,
			u32 output_len, u16 out_pos) {
	u8 ret = 0;
	while (ret < 255 && ret + in_pos < in_len &&
	       out_pos + ret < output_len &&
	       input[in_pos + ret] == output[out_pos + ret]) {
		ret++;
	}

	return ret;
}
*/

STATIC u8 lzx_match_len(const u8 *input, u16 in_len, u16 in_pos, u8 *output,
			u32 output_len, u16 out_pos) {
	u8 ret = 0;
	u16 in_idx;
	u32 out_idx;
	u8 match_len;
	u16 match_offset;

	in_idx = in_pos;
	out_idx = out_pos;

	/* Base case: out of bounds or max match length reached */
	while (ret < 255 && in_idx < in_len && out_idx < output_len) {
		/* Handle MATCH_SENTINEL in output buffer */
		if (output[out_idx] == MATCH_SENTINEL) {
			/* Check for incomplete sequence */
			if (out_idx + 1 >= output_len) {
				break; /* Incomplete sequence, stop matching */
			}

			/* Escaped sentinel */
			if (output[out_idx + 1] == 0x00) {
				if (input[in_idx] != MATCH_SENTINEL) {
					break; /* Mismatch, stop matching */
				}
				ret++;
				in_idx++;
				out_idx += 2; /* Skip 0xFD 0x00 */
				continue;
			}

			/* Match sequence */
			if (out_idx + 3 >= output_len) {
				break; /* Incomplete match sequence, stop
					  matching */
			}
			match_len = output[out_idx + 1];
			match_offset =
			    output[out_idx + 2] | (output[out_idx + 3] << 8);

			/* Validate offset and length */
			if (match_offset >= output_len ||
			    match_len < MIN_MATCH) {
				break; /* Invalid offset or length, stop
					  matching */
			}

			/* Recursively match the referenced sequence */
			if (in_idx + match_len <= in_len) {
				u8 sub_match =
				    lzx_match_len(input, in_len, in_idx, output,
						  output_len, match_offset);
				if (sub_match == match_len) {
					if ((u16)ret + (u16)match_len > 255) {
						ret = 255;
						break;
					}
					ret += match_len;
					in_idx += match_len;
					out_idx +=
					    4; /* Skip 0xFD, len, offset */
					continue;
				}
			}
			break; /* Sub-match failed or too long, stop matching */
		}

		/* Literal character */
		if (input[in_idx] != output[out_idx]) {
			break; /* Mismatch, stop matching */
		}
		ret++;
		in_idx++;
		out_idx++;
	}

	return ret;
}

i32 lzx_compress_block(const u8 *input, u16 in_len, u8 *output,
		       u64 out_capacity) {
	LzxHash hash;
	u32 count = 0;
	u32 itt = 0, i;
	if (!input || !output || !in_len || !out_capacity) {
		err = EINVAL;
		return -1;
	}

	if (count) {
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
				j = lzx_match_len(input, in_len, i, output, itt,
						  value);

				if (j < MIN_MATCH) {
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

					/*
					println("match[{}] found off={},len={}",
						++count, i, j);
						*/

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

