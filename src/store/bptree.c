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

#include <alloc.H>
#include <atomic.H>
#include <bptree.H>
#include <error.H>
#include <format.H>
#include <misc.H>
#include <sys.H>

#define MIN_SIZE (NODE_SIZE * 4)
#define BASE_PTR(txn) ((uint8_t *)txn->tree->base)
#define RUNTIME_NODE(txn) ((RuntimeNode *)(BASE_PTR(txn) + (NODE_SIZE * 2)))
#define METADATA1(tree) ((MetadataNode *)tree->base)
#define METADATA2(tree) ((MetadataNode *)(((uint8_t *)tree->base + NODE_SIZE)))
#define FREE_LIST_NODE(tree) \
	((FreeList *)(((uint8_t *)tree->base + 3 * NODE_SIZE)))
#define NODE_OFFSET(index) (index * NODE_SIZE)
#define NODE(txn, index) \
	((BpTreeNode *)(((uint8_t *)txn->tree->base) + NODE_SIZE * index))
#define FIRST_NODE 4

#define STATIC_ASSERT(condition, message) \
	typedef char static_assert_##message[(condition) ? 1 : -1]

STATIC_ASSERT(sizeof(BpTreeNode) == NODE_SIZE, bptree_node_size);
STATIC_ASSERT(sizeof(BpTreeLeafNode) == sizeof(BpTreeInternalNode),
	      bptree_nodes_equal);

struct BpTree {
	void *base;
	uint64_t capacity;
	int fd;
};

struct BpTxn {
	BpTree *tree;
	uint64_t txn_id;
	uint64_t root;
};

typedef struct {
	uint64_t counter;
	uint64_t root;
} MetadataNode;

typedef struct {
	uint64_t next_txn_id;
} RuntimeNode;

typedef struct {
	uint64_t head;
	uint64_t next_file_node;
} FreeList;

STATIC void bptree_ensure_init(BpTree *tree) {
	MetadataNode *m1 = METADATA1(tree);
	FreeList *fl = FREE_LIST_NODE(tree);
	uint64_t expected = 0;
	__cas64(&m1->root, &expected, FIRST_NODE);
	expected = 0;
	__cas64(&m1->counter, &expected, 1);
	expected = 0;
	__cas64(&fl->next_file_node, &expected, FIRST_NODE + 1);
}

STATIC void shift_by_offset(BpTreeNode *node, uint16_t key_index,
			    uint16_t shift) {
	int i;
	uint16_t pos = node->data.leaf.entry_offsets[key_index];
	uint16_t bytes_to_move = node->used_bytes - pos;
	void *dst = node->data.leaf.entries + pos + shift;
	void *src = node->data.leaf.entries + pos;

	memorymove(dst, src, bytes_to_move);
	for (i = node->num_entries; i > key_index; i--) {
		node->data.leaf.entry_offsets[i] =
		    node->data.leaf.entry_offsets[i - 1] + shift;
	}
}

STATIC void place_entry(BpTreeNode *node, uint16_t key_index, const void *key,
			uint16_t key_len, const void *value,
			uint32_t value_len) {
	uint16_t pos = node->data.leaf.entry_offsets[key_index];
	BpTreeEntry entry = {0};
	entry.value_len = value_len;
	entry.key_len = key_len;
	memcpy((uint8_t *)node->data.leaf.entries + pos, &entry,
	       sizeof(BpTreeEntry));
	memcpy((uint8_t *)node->data.leaf.entries + pos + sizeof(BpTreeEntry),
	       key, key_len);
	memcpy((uint8_t *)node->data.leaf.entries + pos + sizeof(BpTreeEntry) +
		   key_len,
	       value, value_len);
}

STATIC BpTreeNode *allocate_node(BpTxn *txn) {
	FreeList *free_list;
	uint64_t expected, current;
	if (!txn) return NULL;

	free_list = FREE_LIST_NODE(txn->tree);
	do {
		current = ALOAD(&free_list->next_file_node);
		if (current * NODE_SIZE >= txn->tree->capacity) {
			err = ENOMEM;
			return NULL;
		}
		expected = current;
	} while (!__cas64(&free_list->next_file_node, &expected, current + 1));
	return NODE(txn, current);
}

BpTree *bptree_open(const char *path) {
	BpTree *ret;
	int fd = file(path);
	uint64_t capacity;

	if (fd < 0) return NULL;
	if ((capacity = fsize(fd)) < MIN_SIZE) {
		err = EINVAL;
		close(fd);
		return NULL;
	}

	ret = alloc(sizeof(BpTree));
	if (!ret) {
		close(fd);
		return NULL;
	}

	ret->base = fmap(fd, capacity, 0);
	if (!ret->base) {
		release(ret);
		close(fd);
		return NULL;
	}

	ret->fd = fd;
	ret->capacity = capacity;

	bptree_ensure_init(ret);

	return ret;
}

int bptree_close(BpTree *tree) {
	if (!tree) {
		err = EINVAL;
		return -1;
	}
	if (munmap(tree->base, tree->capacity) < 0) return -1;
	return close(tree->fd);
}

BpTxn *bptxn_start(BpTree *tree) {
	BpTxn *ret;
	MetadataNode *m1, *m2;
	uint64_t m1_counter, m2_counter;
	if (!tree) {
		err = EINVAL;
		return NULL;
	}

	ret = alloc(sizeof(BpTxn));
	if (ret == NULL) return NULL;
	ret->tree = tree;
	ret->txn_id = __add64(&(RUNTIME_NODE(ret)->next_txn_id), 1);

	m1 = METADATA1(tree);
	m2 = METADATA2(tree);

	/* Get current root */
	do {
		m1_counter = ALOAD(&m1->counter);
		m2_counter = ALOAD(&m2->counter);
		if (m1_counter > m2_counter)
			ret->root = ALOAD(&m1->root);
		else
			ret->root = ALOAD(&m2->root);
	} while (m1_counter != ALOAD(&m1->counter) ||
		 m2_counter != ALOAD(&m2->counter));

	return ret;
}

int bptxn_commit(BpTxn *txn);
int bptxn_abort(BpTxn *txn);

int bptree_put(BpTxn *txn, const void *key, uint16_t key_len, const void *value,
	       uint32_t value_len, const BpTreeSearch search) {
	BpTreeSearchResult res;
	BpTreeNode *node, *copy;
	uint64_t space_needed;

	if (key_len == 0 || value_len == 0 || key == NULL || value == NULL ||
	    txn == NULL) {
		err = EINVAL;
		return -1;
	}

	node = NODE(txn, txn->root);
	search(txn, key, key_len, node, &res);
	if (res.found) return -1; /* Duplicate */

	space_needed = key_len + value_len + sizeof(BpTreeEntry);
	if (space_needed + node->used_bytes > LEAF_ARRAY_SIZE ||
	    (uint64_t)(node->num_entries + 1) >= MAX_LEAF_ENTRIES) {
		/* TODO */
	} else {
		copy = allocate_node(txn);
		if (!copy) return -1;
		memcpy(copy, node, NODE_SIZE);
		txn->root = bptree_node_id(txn, copy);

		if (copy->num_entries > res.key_index)
			shift_by_offset(copy, res.key_index, space_needed);
		else {
			copy->data.leaf.entry_offsets[copy->num_entries] =
			    copy->used_bytes;
		}

		place_entry(copy, res.key_index, key, key_len, value,
			    value_len);
		copy->used_bytes += space_needed;
		copy->num_entries++;
	}

	return 0;
}

BpTreeEntry *bptree_remove(BpTxn *txn, const void *key, uint16_t key_len,
			   const void *value, uint64_t value_len,
			   const BpTreeSearch search);

BpTreeNode *bptxn_get_node(BpTxn *txn, uint64_t node_id) {
	BpTreeNode *ret = NODE(txn, node_id);
	if ((((uint8_t *)ret) - ((uint8_t *)txn->tree->base)) + NODE_SIZE >
	    txn->tree->capacity)
		return NULL;
	return ret;
}
BpTreeNode *bptree_root(BpTxn *txn) {
	BpTreeNode *ret = NODE(txn, txn->root);
	return ret;
}

uint64_t bptree_node_id(BpTxn *txn, const BpTreeNode *node) {
	return ((uint8_t *)node - (uint8_t *)txn->tree->base) / NODE_SIZE;
}
