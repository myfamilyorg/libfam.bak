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
#include <format.H>
#include <limits.H>
#include <lzx.H>

#define LZX_HASH_ENTRIES 4096
#define HASH_CONSTANT 2654435761U
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

STATIC i32 lzx_decompress_block_impl(const u8 *input, u16 in_len, u16 in_start,
				     u8 *output, u64 out_start,
				     u64 out_capacity, u64 limit) {
	u32 i;
	u64 itt = out_start;
	if (!input || !output || !in_len || !limit ||
	    out_capacity <= out_start) {
		err = EINVAL;
		return -1;
	}

	for (i = in_start; i < in_len; i++) {
		if ((input[i] & 0x80) == 0)
			output[itt++] = input[i];
		else {
			if (i + 1 >= in_len) {
				err = EOVERFLOW;
				return -1;
			}
			if (input[i + 1] == 0xFF) {
				output[itt++] = input[i];
				i++; /* advance past escape */
			} else {
				i32 res;
				u8 len = input[i] & ~0x80;
				u16 offset;
				u64 remaining_limit = limit - (itt - out_start);
				if (i + 2 >= in_len) {
					err = EOVERFLOW;
					return -1;
				}
				offset = (input[i + 1] & 0xFF) | input[i + 2]
								     << 8;
				if (offset >= in_len) {
					err = EINVAL;
					return -1;
				}
				if (len < MIN_MATCH) {
					err = EPROTO;
					return -1;
				}
				if (itt + len > out_capacity) {
					err = EOVERFLOW;
					return -1;
				}
				res = lzx_decompress_block_impl(
				    input, in_len, offset, output, itt,
				    out_capacity,
				    len < remaining_limit ? len
							  : remaining_limit);
				if (res < 0) return -1;
				itt += len;
				i += 2; /* advance past tuple */
			}
		}
		if (itt - out_start >= limit) break;
		if (itt >= out_capacity) {
			err = EOVERFLOW;
			return -1;
		}
	}

	return itt;
}

STATIC u8 lzx_match_len(const u8 *input, u32 in_len, u16 in_pos, u8 *output,
			u32 output_len, u16 out_pos) {
	u8 ret = 0;
	u16 in_idx;
	u32 out_idx;
	u8 match_len;
	u16 match_offset;

	in_idx = in_pos;
	out_idx = out_pos;

	/* Base case: out of bounds or max match length reached */
	while (ret < 127 && in_idx < in_len && out_idx < output_len) {
		/* Handle MATCH_SENTINEL in output buffer */
		if (output[out_idx] & 0x80) {
			/* Check for incomplete sequence */
			if (out_idx + 1 >= output_len) {
				break; /* Incomplete sequence, stop matching */
			}

			/* Escaped sentinel */
			if (output[out_idx + 1] == 0xFF) {
				if ((input[in_idx] & 0x80) == 0) {
					break; /* Mismatch, stop matching */
				}
				ret++;
				in_idx++;
				out_idx += 2; /* Skip 0xFD 0xFF */
				continue;
			}

			/* Match sequence */
			if (out_idx + 2 >= output_len) {
				break; /* Incomplete match sequence, stop
					  matching */
			}
			match_len = output[out_idx] & ~0x80;
			match_offset =
			    output[out_idx + 1] | (output[out_idx + 2] << 8);

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
					if ((u16)ret + (u16)match_len > 127) {
						ret = 127;
						break;
					}
					ret += match_len;
					in_idx += match_len;
					out_idx +=
					    3; /* Skip 0xFD, len, offset */
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
	i32 itt = 0, i;
	u32 key;

	if (!input || !output || !in_len || !out_capacity) {
		err = EINVAL;
		return -1;
	}

	lzx_hash_init(&hash);

	for (i = 0; i < in_len; i++) {
		if (i + sizeof(u32) <= in_len) {
			key = *(u32 *)(input + i);
			u16 value = lzx_hash_get(&hash, key);
			if (value >= itt) {
				bool is_sentinel = input[i] & 0x80;
				if ((u64)(itt + (is_sentinel ? 2 : 1)) >
				    out_capacity) {
					err = ENOBUFS;
					return -1;
				}
				output[itt++] = input[i];
				if (is_sentinel) output[itt++] = 0xFF;
				if (itt >= 4 && (output[itt - 4] & 0x80) == 0 &&
				    (output[itt - 3] & 0x80) == 0 &&
				    (output[itt - 2] & 0x80) == 0 &&
				    (output[itt - 1] & 0x80) == 0) {
					key = *(u32 *)(output + (itt - 4));
					lzx_hash_set(&hash, key, itt - 4);
				}
			} else {
				u32 match_len = lzx_match_len(
				    input, in_len, i, output, itt, value);
				/* Check special case, we can't match 0xFF
				 * because it is the marker to indicate escaped
				 * sentinel. It's ok, very last part of largest
				 * blocks rarely matched. */
				if (match_len < MIN_MATCH ||
				    (value & 0xFF) == 0xFF) {
					bool is_sentinel = input[i] & 0x80;
					if ((u64)(itt + (is_sentinel ? 2 : 1)) >
					    out_capacity) {
						err = ENOBUFS;
						return -1;
					}
					output[itt++] = input[i];
					if (is_sentinel) output[itt++] = 0xFF;
				} else {
					if ((u64)(itt + 3) >= out_capacity) {
						err = ENOBUFS;
						return -1;
					}

					output[itt++] = 0x80 | match_len;
					output[itt++] = value & 0xFF;
					output[itt++] = value >> 8;
					i += match_len - 1;
				}
			}
		} else {
			bool is_sentinel = input[i] & 0x80;
			if ((u64)(itt + (is_sentinel ? 2 : 1)) > out_capacity) {
				err = ENOBUFS;
				return -1;
			}
			output[itt++] = input[i];
			if (is_sentinel) output[itt++] = 0xFF;
			if (itt >= 4 && (output[itt - 4] & 0x80) == 0 &&
			    (output[itt - 3] & 0x80) == 0 &&
			    (output[itt - 2] & 0x80) == 0 &&
			    (output[itt - 1] & 0x80) == 0) {
				key = *(u32 *)(output + (itt - 4));
				lzx_hash_set(&hash, key, itt - 4);
			}
		}
	}

	return itt;
}

i32 lzx_decompress_block(const u8 *input, u32 in_len, u8 *output,
			 u64 out_capacity) {
	return lzx_decompress_block_impl(input, in_len, 0, output, 0,
					 out_capacity, U64_MAX);
}

