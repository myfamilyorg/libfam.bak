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
#include <huffman.H>

typedef struct HuffmanNode {
	u8 symbol;
	u64 freq;
	struct HuffmanNode *left, *right;
} HuffmanNode;

typedef struct {
	HuffmanNode *nodes[MAX_HUFFMAN_SYMBOLS];
	u64 size;
	u64 capacity;
} HuffmanMinHeap;

bool is_prefix_free(HuffmanLookup *lookup) {
	int i, j;
	for (i = 0; i < 256; i++) {
		if (!lookup->lengths[i]) continue;
		for (j = 0; j < 256; j++) {
			if (i == j || !lookup->lengths[j]) continue;
			if (lookup->lengths[i] <= lookup->lengths[j]) {
				u32 code_i = lookup->codes[i];
				u32 code_j = lookup->codes[j];
				u32 mask = (1u << lookup->lengths[i]) - 1;
				if ((code_j & mask) == code_i) {
					u32 full_mask =
					    (1u << lookup->lengths[j]) - 1;
					u32 shifted_code_i =
					    code_i << (lookup->lengths[j] -
						       lookup->lengths[i]);
					if ((code_j & full_mask) ==
					    shifted_code_i) {
						println(
						    "Code 0x{X} (len={}) is "
						    "prefix of 0x{X} (len={})",
						    code_i, lookup->lengths[i],
						    code_j, lookup->lengths[j]);
						return false;
					}
				}
			}
		}
	}
	return true;
}

STATIC void print_tree(HuffmanNode *node, int depth) {
	int i;
	if (!node) return;
	for (i = 0; i < depth; i++) print("  ");
	if (!node->left && !node->right) {
		println("Leaf: {x} ({c}), freq={}", node->symbol, node->symbol,
			node->freq);
	} else
		println("Internal: freq={}", node->freq);
	print_tree(node->left, depth + 1);
	print_tree(node->right, depth + 1);
}

STATIC void huffman_init_node(HuffmanNode *node, u8 symbol, u64 freq) {
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

HuffmanNode *build_huffman_tree(HuffmanMinHeap *heap, u64 *freq,
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
		huffman_init_node(parent, '$', left->freq + right->freq);
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

const u8 *huffman_lookup_compress(const HuffmanLookup *lookup, u16 *size);
i32 huffmean_lookup_decompress(HuffmanLookup *lookup, const u8 *compressed,
			       u16 size);

i32 huffman_gen(HuffmanLookup *lookup, const u8 *input, u16 len) {
	HuffmanMinHeap heap;
	HuffmanNode *root;
	u64 freq[MAX_HUFFMAN_SYMBOLS] = {0};
	u8 stack_ptr[sizeof(HuffmanNode) * MAX_HUFFMAN_SYMBOLS * 2];
	u16 i;

	if (!lookup || !input || !len) {
		err = EINVAL;
		return -1;
	}
	for (i = 0; i < len; i++) freq[input[i]]++;
	root = build_huffman_tree(&heap, freq, stack_ptr);
	huffman_generate_codes(root, 0, 0, lookup);

	return 0;
}

