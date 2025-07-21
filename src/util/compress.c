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

#include <libfam/compress.H>
#include <libfam/error.H>
#include <libfam/format.H>
#include <libfam/huffman.H>
#include <libfam/limits.H>

#define LZX_HASH_ENTRIES 4096
#define HASH_CONSTANT 0x9e3779b97f4a7c15ULL
#define MIN_MATCH 6
#define MAX_MATCH 127
#define MATCH_SENTINEL 0x80
#define ESC 0xFF
#define BLOCK_SIZE 32767U

#define APPEND_SYMBOL                                            \
	bool is_sentinel = input[i] & MATCH_SENTINEL;            \
	if ((u64)(itt + (is_sentinel ? 2 : 1)) > out_capacity) { \
		err = ENOBUFS;                                   \
		return -1;                                       \
	}                                                        \
	output[itt++] = input[i];                                \
	if (is_sentinel) output[itt++] = ESC;

#define SET_HASH                                                               \
	if (itt >= 6) {                                                        \
		bool can_set = true;                                           \
		if ((output[itt - 6] & MATCH_SENTINEL) != 0 &&                 \
		    output[itt - 5] != 0xFF)                                   \
			can_set = false;                                       \
		if ((output[itt - 5] & MATCH_SENTINEL) != 0 &&                 \
		    output[itt - 4] != 0xFF)                                   \
			can_set = false;                                       \
		if ((output[itt - 4] & MATCH_SENTINEL) != 0 &&                 \
		    output[itt - 3] != 0xFF)                                   \
			can_set = false;                                       \
		if ((output[itt - 3] & MATCH_SENTINEL) != 0 &&                 \
		    output[itt - 2] != 0xFF)                                   \
			can_set = false;                                       \
		if ((output[itt - 2] & MATCH_SENTINEL) != 0 &&                 \
		    output[itt - 1] != 0xFF)                                   \
			can_set = false;                                       \
		if ((output[itt - 1] & MATCH_SENTINEL) != 0 &&                 \
		    output[itt] != 0xFF)                                       \
			can_set = false;                                       \
		if (can_set) {                                                 \
			key = *(u32 *)(output + (itt - 6)) |                   \
			      ((u64)(*(u16 *)(output + (itt - 6 + 4))) << 32); \
			lzx_hash_set(&hash, key, itt - 6);                     \
		}                                                              \
	}

typedef struct HuffmanNode {
	u16 symbol;
	u64 freq;
	struct HuffmanNode *left, *right;
} HuffmanNode;

typedef struct {
	HuffmanNode *nodes[MAX_HUFFMAN_SYMBOLS];
	u64 size;
	u64 capacity;
} HuffmanMinHeap;

typedef struct {
	u8 table[LZX_HASH_ENTRIES * 2] __attribute__((aligned(16)));
} LzxHash;

STATIC void huffman_init_node(HuffmanNode *node, u16 symbol, u64 freq) {
	node->symbol = symbol;
	node->freq = freq;
	node->left = node->right = NULL;
}

STATIC void huffman_init_heap(HuffmanMinHeap *heap) { heap->size = 0; }

STATIC void huffman_swap_nodes(HuffmanNode **a, HuffmanNode **b) {
	HuffmanNode *temp = *a;
	*a = *b;
	*b = temp;
}

STATIC void huffman_heapify(HuffmanMinHeap *heap, u64 idx) {
	u64 smallest = idx;
	u64 left = 2 * idx + 1;
	u64 right = 2 * idx + 2;

	if (left < heap->size &&
	    heap->nodes[left]->freq < heap->nodes[smallest]->freq)
		smallest = left;
	if (right < heap->size &&
	    heap->nodes[right]->freq < heap->nodes[smallest]->freq)
		smallest = right;

	if (smallest != idx) {
		huffman_swap_nodes(&heap->nodes[idx], &heap->nodes[smallest]);
		huffman_heapify(heap, smallest);
	}
}

STATIC void huffman_insert_heap(HuffmanMinHeap *heap, HuffmanNode *node) {
	u64 i = ++heap->size - 1;
	heap->nodes[i] = node;

	while (i && heap->nodes[(i - 1) / 2]->freq > heap->nodes[i]->freq) {
		huffman_swap_nodes(&heap->nodes[i], &heap->nodes[(i - 1) / 2]);
		i = (i - 1) / 2;
	}
}

STATIC HuffmanNode *huffman_extract_min(HuffmanMinHeap *heap) {
	HuffmanNode *min;
	if (heap->size == 0) return NULL;

	min = heap->nodes[0];
	heap->nodes[0] = heap->nodes[heap->size - 1];
	heap->size--;
	huffman_heapify(heap, 0);

	return min;
}

STATIC HuffmanNode *build_huffman_tree(HuffmanMinHeap *heap, u64 *freq,
				       u8 *stack_ptr) {
	u16 i;
	u16 node_counter = 0;

	huffman_init_heap(heap);

	for (i = 0; i < MAX_HUFFMAN_SYMBOLS; i++)
		if (freq[i] > 0) {
			HuffmanNode *next =
			    (HuffmanNode *)(stack_ptr + sizeof(HuffmanNode) *
							    node_counter++);
			huffman_init_node(next, (u8)i, freq[i]);
			huffman_insert_heap(heap, next);
		}

	while (heap->size > 1) {
		HuffmanNode *left = huffman_extract_min(heap);
		HuffmanNode *right = huffman_extract_min(heap);
		HuffmanNode *parent =
		    (HuffmanNode *)(stack_ptr +
				    sizeof(HuffmanNode) * node_counter++);
		huffman_init_node(parent, 0xFFFF, left->freq + right->freq);
		parent->left = left;
		parent->right = right;
		huffman_insert_heap(heap, parent);
	}

	return huffman_extract_min(heap);
}

STATIC void huffman_generate_codes(HuffmanNode *root, u32 code, u8 length,
				   HuffmanLookup *lookup) {
	if (!root) return;
	if (!root->left && !root->right) {
		lookup->codes[root->symbol] = code;
		lookup->lengths[root->symbol] = length;
	}
	huffman_generate_codes(root->left, code << 1, length + 1, lookup);
	huffman_generate_codes(root->right, (code << 1) | 1, length + 1,
			       lookup);
}

STATIC i32 huffman_gen(HuffmanLookup *lookup, const u8 *input, u16 len) {
	HuffmanMinHeap heap;
	HuffmanNode *root;
	u64 freq[MAX_HUFFMAN_SYMBOLS] = {0};
	u8 stack_ptr[sizeof(HuffmanNode) * MAX_HUFFMAN_SYMBOLS * 2];
	u16 i;

	if (!lookup || !input || !len) {
		err = EINVAL;
		return -1;
	}
	lookup->count = 0;

	for (i = 0; i < len; i++) {
		if (!freq[input[i]]) lookup->count++;
		freq[input[i]]++;
	}

	root = build_huffman_tree(&heap, freq, stack_ptr);
	huffman_generate_codes(root, 0, 0, lookup);

	return 0;
}

STATIC HuffmanNode *huffman_reconstruct_tree(HuffmanLookup *lookup,
					     u8 *stack_ptr) {
	u16 node_counter = 0, i;
	u8 j;
	HuffmanNode *root =
	    (HuffmanNode *)(stack_ptr + sizeof(HuffmanNode) * node_counter++);
	root->symbol = 0xFFFF;
	root->freq = 0;
	root->left = NULL;
	root->right = NULL;

	for (i = 0; i < 256; i++) {
		if (lookup->lengths[i] == 0) continue;
		u32 code = lookup->codes[i];
		u8 len = lookup->lengths[i];
		HuffmanNode *current = root;

		for (j = 0; j < len; j++) {
			u8 bit = (code >> (len - 1 - j)) & 1;
			if (bit == 0) {
				if (!current->left) {
					current->left =
					    (HuffmanNode *)(stack_ptr +
							    sizeof(
								HuffmanNode) *
								node_counter++);
					current->left->symbol = 0xFFFF;
					current->left->freq = 0;
					current->left->left = NULL;
					current->left->right = NULL;
				}
				current = current->left;
			} else {
				if (!current->right) {
					current->right =
					    (HuffmanNode *)(stack_ptr +
							    sizeof(
								HuffmanNode) *
								node_counter++);
					current->right->symbol = 0xFFFF;
					current->right->freq = 0;
					current->right->left = NULL;
					current->right->right = NULL;
				}
				current = current->right;
			}
		}
		current->symbol = (u8)i;
	}

	return root;
}

STATIC i32 huffman_decode(const u8 *input, u32 len, u8 *output,
			  u32 output_capacity) {
	if (!input || len < 2 || !output || !output_capacity) {
		err = EINVAL;
		return -1;
	}

	HuffmanLookup huffman = {0};
	u16 i;
	u16 expected_symbols = ((u16)input[0] << 8) | input[1];
	if (expected_symbols > output_capacity) {
		err = EOVERFLOW;
		return -1;
	}
	huffman.count = input[2];
	for (i = 0; i < huffman.count; i++) {
		u8 index = *(u8 *)(input + 3 + i * 7);
		huffman.lengths[index] = *(u8 *)(input + 3 + i * 7 + 1);
		huffman.codes[index] = *(u32 *)(input + 3 + i * 7 + 2);
	}
	u8 stack[MAX_HUFFMAN_SYMBOLS * 2 * sizeof(HuffmanNode)];
	HuffmanNode *root = huffman_reconstruct_tree(&huffman, stack);
	if (!root) {
		err = EINVAL;
		return -1;
	}

	u32 bit_pos = 0;
	u32 byte_pos = 3 + huffman.count * 7;
	u32 output_pos = 0;
	HuffmanNode *current = root;

	while (output_pos < expected_symbols && bit_pos < (len - 2) * 8) {
		if (byte_pos >= len) {
			err = EINVAL;
			return -1;
		}

		u8 bit = (input[byte_pos] >> (7 - (bit_pos % 8))) & 1;
		bit_pos++;
		current = bit ? current->right : current->left;

		if (!current) {
			err = EINVAL;
			return -1;
		}

		if (!current->left && !current->right &&
		    current->symbol != 0xFFFF) {
			output[output_pos++] = current->symbol;
			current = root;
		}

		if (bit_pos % 8 == 0) {
			byte_pos++;
		}
	}

	if (output_pos != expected_symbols) {
		err = EINVAL;
		return -1;
	}

	if (current != root) {
		err = EINVAL;
		return -1;
	}

	return (i32)output_pos;
}

STATIC i32 huffman_pc(u32 code, u8 code_len, u32 *bit_pos, u32 *byte_pos,
		      u8 *current_byte, u8 *output, u64 output_capacity) {
	u8 j;
	for (j = 0; j < code_len; j++) {
		if (*byte_pos >= output_capacity) {
			err = EOVERFLOW;
			return -1;
		}

		u32 bit = (code >> (code_len - 1 - j)) & 1;
		*current_byte |= bit << (7 - (*bit_pos % 8));
		(*bit_pos)++;

		if ((*bit_pos) % 8 == 0) {
			output[(*byte_pos)++] = *current_byte;
			*current_byte = 0;
		}
	}
	return 0;
}

STATIC i32 huffman_encode(const u8 *input, u16 len, u8 *out, u32 cap) {
	HuffmanLookup huffman = {0};
	if (!input || !len || !out || cap < 3) {
		err = EINVAL;
		return -1;
	}

	huffman_gen(&huffman, input, len);
	if (cap < (u32)3 + huffman.count * 7) {
		err = EOVERFLOW;
		return -1;
	}

	out[0] = (len >> 8) & 0xFF;
	out[1] = len & 0xFF;
	out[2] = huffman.count;
	u32 byp = 3 + huffman.count * 7;
	u32 bip = 0;
	u8 cb = 0;
	u16 i;
	u8 j = 0;

	for (i = 0; i < 256; i++) {
		if (huffman.lengths[i]) {
			u8 v = (u8)i;
			memcpy(out + 3 + j * 7, &v, sizeof(u8));
			memcpy(out + 3 + j * 7 + 1, &huffman.lengths[i],
			       sizeof(u8));
			memcpy(out + 3 + j * 7 + 2, &huffman.codes[i],
			       sizeof(u32));
			j++;
		}
	}
	for (i = 0; i < len; i++) {
		u8 symbol = input[i];

		if (huffman.lengths[symbol] == 0) {
			err = EINVAL;
			return -1;
		}

		u32 code = huffman.codes[symbol];
		u8 code_len = huffman.lengths[symbol];

		if (huffman_pc(code, code_len, &bip, &byp, &cb, out, cap) < 0)
			return -1;
	}
	if (bip % 8 != 0) {
		if (byp >= cap) {
			err = EOVERFLOW;
			return -1;
		}
		out[byp++] = cb;
	}

	return (i32)byp;
}

STATIC void lzx_hash_init(LzxHash *hash) {
	u32 i;
	for (i = 0; i < LZX_HASH_ENTRIES * 2; i += 2) {
		*(u16 *)(hash->table + i) = U16_MAX;
	}
}

STATIC u16 lzx_hash_get(LzxHash *hash, u64 key) {
	u64 h = (key * HASH_CONSTANT) >> (64 - 12);
	h &= (LZX_HASH_ENTRIES - 1);
	return *(u16 *)(hash->table + h * 2);
}

STATIC void lzx_hash_set(LzxHash *hash, u64 key, u16 value) {
	u64 h = (key * HASH_CONSTANT) >> (64 - 12);
	h &= (LZX_HASH_ENTRIES - 1);
	*(u16 *)(hash->table + h * 2) = value;
}

STATIC u8 lzx_match_len(const u8 *input, u32 in_len, u16 in_pos, u8 *out,
			u32 output_len, u16 out_pos) {
	u8 ret = 0, match_len;
	u16 iidx, match_offset;
	u32 oidx;

	iidx = in_pos;
	oidx = out_pos;

	while (ret < MAX_MATCH && iidx < in_len && oidx < output_len) {
		if (out[oidx] & MATCH_SENTINEL) {
			if (oidx + 1 >= output_len) break;
			if (out[oidx + 1] == ESC) {
				if ((input[iidx] & MATCH_SENTINEL) == 0) break;
				ret++;
				iidx++;
				oidx += 2;
				continue;
			}
			if (oidx + 2 >= output_len) break;
			match_len = out[oidx] & ~MATCH_SENTINEL;
			match_offset = out[oidx + 1] | (out[oidx + 2] << 8);

			if (match_offset >= output_len || match_len < MIN_MATCH)
				break;
			if (iidx + match_len <= in_len) {
				u8 sub_match =
				    lzx_match_len(input, in_len, iidx, out,
						  output_len, match_offset);
				if (sub_match == match_len) {
					if (ret + (u16)match_len > MAX_MATCH) {
						ret = MAX_MATCH;
						break;
					}
					ret += match_len;
					iidx += match_len;
					oidx += 3;
					continue;
				}
			}
			break;
		}
		if (input[iidx] != out[oidx]) break;
		ret++;
		iidx++;
		oidx++;
	}

	return ret;
}

STATIC i32 lzx_decompress_block_impl(const u8 *in, u16 in_len, u16 in_start,
				     u8 *output, u64 out_start,
				     u64 out_capacity, u64 limit) {
	u32 i;
	u64 itt = out_start;
	if (!in || !output || !in_len || !limit || out_capacity <= out_start) {
		err = EINVAL;
		return -1;
	}

	for (i = in_start; i < in_len; i++) {
		if ((in[i] & MATCH_SENTINEL) == 0)
			output[itt++] = in[i];
		else {
			if (i + 1 >= in_len) {
				err = EOVERFLOW;
				return -1;
			}
			if (in[i + 1] == ESC)
				output[itt++] = in[i++];
			else {
				i32 res, rem;
				u8 len = in[i] & ~MATCH_SENTINEL;
				u16 offset;
				u64 rlim = limit - (itt - out_start);
				if (i + 2 >= in_len) {
					err = EOVERFLOW;
					return -1;
				}
				offset = (in[i + 2] & 0xFF) | in[i + 1] << 8;
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
				rem = len < rlim ? len : rlim;
				res = lzx_decompress_block_impl(
				    in, in_len, offset, output, itt,
				    out_capacity, rem);
				if (res < 0) return -1;
				itt += len;
				i += 2;
			}
		}
		if (itt - out_start >= limit) break;
		if (itt > out_capacity) {
			err = EOVERFLOW;
			return -1;
		}
	}

	return itt;
}

STATIC i32 lzx_decompress_block(const u8 *input, u32 in_len, u8 *output,
				u64 out_capacity) {
	return lzx_decompress_block_impl(input, in_len, 0, output, 0,
					 out_capacity, U64_MAX);
}

STATIC i32 lzx_compress_block(const u8 *input, u16 in_len, u8 *output,
			      u64 out_capacity) {
	LzxHash hash;
	i32 itt = 0, i;
	u64 key;

	if (!input || !output || !in_len || !out_capacity) {
		err = EINVAL;
		return -1;
	}

	lzx_hash_init(&hash);

	for (i = 0; i < in_len; i++) {
		if (i + 6 <= in_len) {
			key = *(u32 *)(input + i) |
			      ((u64)(*(u16 *)(input + i + 4)) << 32);
			u16 value = lzx_hash_get(&hash, key);
			if (value >= itt) {
				APPEND_SYMBOL
				SET_HASH
			} else {
				u32 mlen = lzx_match_len(input, in_len, i,
							 output, itt, value);
				if (mlen < MIN_MATCH || (value & 0xFF) == ESC) {
					APPEND_SYMBOL
					SET_HASH
				} else {
					if ((u64)(itt + 3) >= out_capacity) {
						err = ENOBUFS;
						return -1;
					}
					output[itt++] = MATCH_SENTINEL | mlen;
					output[itt++] = value >> 8;
					output[itt++] = value & 0xFF;
					i += mlen - 1;
				}
			}
		} else {
			APPEND_SYMBOL
		}
	}

	return itt;
}

i64 compress(u8 *in, u64 len, u8 *out, u64 capacity) {
	u8 buf[BLOCK_SIZE * 2];
	u64 in_pos = 0;
	u64 out_pos = 0;
	u64 num_blocks;
	i32 res;

	if (!in || !out || !len || !capacity) {
		err = EINVAL;
		return -1;
	}
	if (capacity < sizeof(u64)) {
		err = ENOBUFS;
		return -1;
	}

	num_blocks = len / BLOCK_SIZE + (len % BLOCK_SIZE != 0 ? 1 : 0);
	memcpy(out + out_pos, &num_blocks, sizeof(u64));
	out_pos += sizeof(u64);

	while (in_pos < len) {
		u16 block_len =
		    (u16)((len - in_pos < BLOCK_SIZE) ? (len - in_pos)
						      : BLOCK_SIZE);
		if (out_pos + sizeof(u32) > capacity) {
			err = ENOBUFS;
			return -1;
		}

		res = lzx_compress_block(in + in_pos, block_len, buf,
					 sizeof(buf));
		if (res < 0) return -1;

		if (out_pos + sizeof(u32) + 3 > capacity) {
			err = ENOBUFS;
			return -1;
		}
		i32 comp_len =
		    huffman_encode(buf, (u16)res, out + out_pos + sizeof(u32),
				   (u32)(capacity - out_pos - sizeof(u32)));
		if (comp_len < 0) return -1;

		u32 u_comp_len = (u32)comp_len;
		memcpy(out + out_pos, &u_comp_len, sizeof(u32));
		out_pos += sizeof(u32) + (u64)comp_len;
		in_pos += (u64)block_len;
	}

	return (i64)out_pos;
}

i64 decompress(u8 *in, u64 len, u8 *out, u64 capacity) {
	u8 buf[BLOCK_SIZE * 2];
	u64 in_pos = 0;
	u64 out_pos = 0;
	u64 num_blocks, b;
	i32 res;

	if (!in || !out || !len || !capacity) {
		err = EINVAL;
		return -1;
	}
	if (len < sizeof(u64)) {
		err = EINVAL;
		return -1;
	}

	memcpy(&num_blocks, in + in_pos, sizeof(u64));
	in_pos += sizeof(u64);

	for (b = 0; b < num_blocks; b++) {
		u32 block_comp_len;
		if (in_pos + sizeof(u32) > len) {
			err = EINVAL;
			return -1;
		}
		memcpy(&block_comp_len, in + in_pos, sizeof(u32));
		in_pos += sizeof(u32);
		if (in_pos + (u64)block_comp_len > len) {
			err = EINVAL;
			return -1;
		}

		res = huffman_decode(in + in_pos, block_comp_len, buf,
				     sizeof(buf));
		if (res < 0) return -1;

		i32 decomp_len = lzx_decompress_block(
		    buf, (u32)res, out + out_pos, capacity - out_pos);
		if (decomp_len < 0) return -1;
		if ((u64)decomp_len > capacity - out_pos) {
			err = EOVERFLOW;
			return -1;
		}

		out_pos += (u64)decomp_len;
		in_pos += (u64)block_comp_len;
	}

	return (i64)out_pos;
}
