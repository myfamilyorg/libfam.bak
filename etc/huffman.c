#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SYMBOLS 256

// Structure for Huffman code
typedef struct {
	uint32_t code;	 // Binary code
	uint8_t length;	 // Code length in bits
} HuffmanCode;

// Structure for Huffman tree node
typedef struct Node {
	uint8_t symbol;	 // Byte value
	uint64_t freq;	 // Frequency
	struct Node *left, *right;
} Node;

// Structure for min-heap
typedef struct {
	Node **nodes;
	size_t size;
	size_t capacity;
} MinHeap;

// Create a new node
Node *new_node(uint8_t symbol, uint64_t freq) {
	Node *node = (Node *)malloc(sizeof(Node));
	node->symbol = symbol;
	node->freq = freq;
	node->left = node->right = NULL;
	return node;
}

// Create a min-heap
MinHeap *create_heap(size_t capacity) {
	MinHeap *heap = (MinHeap *)malloc(sizeof(MinHeap));
	heap->size = 0;
	heap->capacity = capacity;
	heap->nodes = (Node **)malloc(capacity * sizeof(Node *));
	return heap;
}

// Swap two nodes in the heap
void swap_nodes(Node **a, Node **b) {
	Node *temp = *a;
	*a = *b;
	*b = temp;
}

// Heapify down
void heapify(MinHeap *heap, size_t idx) {
	size_t smallest = idx;
	size_t left = 2 * idx + 1;
	size_t right = 2 * idx + 2;

	if (left < heap->size &&
	    heap->nodes[left]->freq < heap->nodes[smallest]->freq)
		smallest = left;
	if (right < heap->size &&
	    heap->nodes[right]->freq < heap->nodes[smallest]->freq)
		smallest = right;

	if (smallest != idx) {
		swap_nodes(&heap->nodes[idx], &heap->nodes[smallest]);
		heapify(heap, smallest);
	}
}

// Insert into heap
void insert_heap(MinHeap *heap, Node *node) {
	heap->size++;
	size_t i = heap->size - 1;
	heap->nodes[i] = node;

	while (i && heap->nodes[(i - 1) / 2]->freq > heap->nodes[i]->freq) {
		swap_nodes(&heap->nodes[i], &heap->nodes[(i - 1) / 2]);
		i = (i - 1) / 2;
	}
}

// Extract minimum from heap
Node *extract_min(MinHeap *heap) {
	if (heap->size == 0) return NULL;
	Node *min = heap->nodes[0];
	heap->nodes[0] = heap->nodes[heap->size - 1];
	heap->size--;
	heapify(heap, 0);
	return min;
}

// Build Huffman tree
Node *build_huffman_tree(uint64_t *freq) {
	MinHeap *heap = create_heap(MAX_SYMBOLS);
	for (int i = 0; i < MAX_SYMBOLS; i++) {
		if (freq[i] > 0) {
			insert_heap(heap, new_node((uint8_t)i, freq[i]));
		}
	}

	while (heap->size > 1) {
		Node *left = extract_min(heap);
		Node *right = extract_min(heap);
		Node *parent = new_node('$', left->freq + right->freq);
		parent->left = left;
		parent->right = right;
		insert_heap(heap, parent);
	}

	Node *root = extract_min(heap);
	free(heap->nodes);
	free(heap);
	return root;
}

// Generate Huffman codes
void generate_codes(Node *root, uint32_t code, uint8_t length,
		    HuffmanCode *codes) {
	if (!root) return;
	if (!root->left && !root->right) {
		codes[root->symbol].code = code;
		codes[root->symbol].length = length;
	}
	generate_codes(root->left, code << 1, length + 1, codes);
	generate_codes(root->right, (code << 1) | 1, length + 1, codes);
}

// Free Huffman tree
void free_tree(Node *root) {
	if (!root) return;
	free_tree(root->left);
	free_tree(root->right);
	free(root);
}

int main(int argc, char **argv) {
	// Step 1: Read file and calculate frequencies
	FILE *file = fopen(argv[1], "rb");
	if (!file) {
		printf("Cannot open file\n");
		return 1;
	}

	uint64_t freq[MAX_SYMBOLS] = {0};
	int c, count = 0;
	while ((c = fgetc(file)) != EOF) {
		count++;
		freq[c]++;
	}
	fclose(file);
	for (int i = 0; i < MAX_SYMBOLS; i++) {
		printf("sym[%d]=%ld\n", i, freq[i]);
	}

	for (c = 0; c < MAX_SYMBOLS; c++)
		if (!freq[c]) freq[c]++;
	freq[0xFD] = 258989;

	Node *root = build_huffman_tree(freq);

	HuffmanCode codes[MAX_SYMBOLS] = {0};
	generate_codes(root, 0, 0, codes);

	printf("Huffman Codes:\n");
	for (int i = 0; i < MAX_SYMBOLS; i++) {
		if (codes[i].length > 0) {
			printf("Symbol: %c (0x%02X), Code: ",
			       isprint(i) ? i : '.', i);
			// Print binary
			for (int j = codes[i].length - 1; j >= 0; j--) {
				printf("%d", (codes[i].code >> j) & 1);
			}
			// Print hex
			printf(" (0x%0*X), Length: %d\n",
			       (codes[i].length + 3) /
				   4,  // Number of hex digits needed
			       codes[i].code, codes[i].length);
		}
	}

	printf("u32 huffman_values[%d] = {\n", MAX_SYMBOLS);
	for (int i = 0; i < MAX_SYMBOLS; i++) {
		if (i) printf(",\n");
		printf("\t0x%0*X", (codes[i].length + 3) / 4, codes[i].code);
	}
	printf("\n};\n");
	printf("u8 huffman_lengths[%d] = {\n", MAX_SYMBOLS);
	for (int i = 0; i < MAX_SYMBOLS; i++) {
		if (i) printf(",\n");
		printf("\t%d", codes[i].length);
	}
	printf("\n};\n");

	free_tree(root);
	return 0;
}
