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
#define BASE_PTR(txn) ((u8 *)txn->tree->base)
#define RUNTIME_NODE(txn) ((RuntimeNode *)(BASE_PTR(txn) + (NODE_SIZE * 2)))
#define METADATA1(tree) ((MetadataNode *)tree->base)
#define METADATA2(tree) ((MetadataNode *)(((u8 *)tree->base + NODE_SIZE)))
#define FREE_LIST_NODE(tree) ((FreeList *)(((u8 *)tree->base + 3 * NODE_SIZE)))
#define NODE_OFFSET(index) (index * NODE_SIZE)
#define NODE(txn, index) \
	((BpTreeNode *)(((u8 *)txn->tree->base) + NODE_SIZE * index))
#define FIRST_NODE 4

#define STATIC_ASSERT(condition, message) \
	typedef u8 static_assert_##message[(condition) ? 1 : -1]

STATIC_ASSERT(sizeof(BpTreeNode) == NODE_SIZE, bptree_node_size);
STATIC_ASSERT(sizeof(BpTreeLeafNode) == sizeof(BpTreeInternalNode),
	      bptree_nodes_equal);

struct BpTree {
	void *base;
	u64 capacity;
	int fd;
};

struct BpTxn {
	BpTree *tree;
	u64 txn_id;
	u64 root;
};

typedef struct {
	u64 counter;
	u64 root;
} MetadataNode;

typedef struct {
	u64 next_txn_id;
} RuntimeNode;

typedef struct {
	u64 head;
	u64 next_file_node;
} FreeList;

STATIC void bptree_ensure_init(BpTree *tree) {
	MetadataNode *m1 = METADATA1(tree);
	FreeList *fl = FREE_LIST_NODE(tree);
	u64 expected = 0;
	__cas64(&m1->root, &expected, FIRST_NODE);
	expected = 0;
	__cas64(&m1->counter, &expected, 1);
	expected = 0;
	__cas64(&fl->next_file_node, &expected, FIRST_NODE + 1);
}

STATIC void shift_by_offset(BpTreeNode *node, u16 key_index, u16 shift) {
	int i;
	u16 pos = node->data.leaf.entry_offsets[key_index];
	u16 bytes_to_move = node->used_bytes - pos;
	void *dst = node->data.leaf.entries + pos + shift;
	void *src = node->data.leaf.entries + pos;

	memorymove(dst, src, bytes_to_move);
	for (i = node->num_entries; i > key_index; i--) {
		node->data.leaf.entry_offsets[i] =
		    node->data.leaf.entry_offsets[i - 1] + shift;
	}
}

STATIC void place_entry(BpTreeNode *node, u16 key_index, const void *key,
			u16 key_len, const void *value, u32 value_len) {
	u16 pos = node->data.leaf.entry_offsets[key_index];
	BpTreeEntry entry = {0};
	entry.value_len = value_len;
	entry.key_len = key_len;
	memcpy((u8 *)node->data.leaf.entries + pos, &entry,
	       sizeof(BpTreeEntry));
	memcpy((u8 *)node->data.leaf.entries + pos + sizeof(BpTreeEntry), key,
	       key_len);
	memcpy(
	    (u8 *)node->data.leaf.entries + pos + sizeof(BpTreeEntry) + key_len,
	    value, value_len);
}

STATIC BpTreeNode *allocate_node(BpTxn *txn) {
	FreeList *free_list;
	u64 expected, current;
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

STATIC void insert_entry(BpTreeNode *node, u16 key_index, u16 space_needed,
			 const void *key, u16 key_len, const void *value,
			 u32 value_len) {
	if (node->num_entries > key_index)
		shift_by_offset(node, key_index, space_needed);
	else {
		node->data.leaf.entry_offsets[node->num_entries] =
		    node->used_bytes;
	}

	place_entry(node, key_index, key, key_len, value, value_len);
	node->used_bytes += space_needed;
	node->num_entries++;
}

STATIC BpTreeNode *new_node(BpTxn *txn, BpTreeNode *node, u16 midpoint) {
	BpTreeNode *new = allocate_node(txn);
	BpTreeNode *nparent = allocate_node(txn);
	BpTreeInternalEntry ent;
	u16 mid_offset = node->data.leaf.entry_offsets[midpoint];
	BpTreeEntry *mident =
	    (BpTreeEntry *)(((u8 *)node->data.leaf.entries) + mid_offset);
	BpTreeEntry *zeroent = (BpTreeEntry *)((u8 *)node->data.leaf.entries);
	if (!nparent || !new) return NULL; /* TODO: release other */

	printf("new node!\n");

	nparent->is_internal = true;
	nparent->parent_id = 0; /* Set to root for now */
	nparent->num_entries = 2;
	nparent->used_bytes = mident->key_len + zeroent->key_len +
			      sizeof(BpTreeInternalEntry) * 2;
	nparent->data.internal.entry_offsets[0] = 0;
	nparent->data.internal.entry_offsets[1] =
	    zeroent->key_len + sizeof(BpTreeInternalEntry);
	printf("setting offset=%u\n", nparent->data.internal.entry_offsets[1]);

	txn->root = bptree_node_id(txn, nparent);
	new->parent_id = txn->root;
	node->parent_id = txn->root;

	ent.key_len = zeroent->key_len;
	ent.node_id = bptree_node_id(txn, node);
	printf("ent->node_id=%i\n", ent.node_id);

	memcpy((u8 *)nparent->data.internal.key_entries, &ent,
	       sizeof(BpTreeInternalEntry));
	memcpy((u8 *)nparent->data.internal.key_entries +
		   sizeof(BpTreeInternalEntry),
	       (u8 *)zeroent + sizeof(BpTreeEntry), zeroent->key_len);

	ent.key_len = mident->key_len;
	ent.node_id = bptree_node_id(txn, new);
	printf("new ent->node_id=%i,offsetent=%i\n", ent.node_id,
	       sizeof(BpTreeInternalEntry) + zeroent->key_len);

	memcpy((u8 *)nparent->data.internal.key_entries +
		   sizeof(BpTreeInternalEntry) + zeroent->key_len,
	       &ent, sizeof(BpTreeInternalEntry));
	memcpy((u8 *)nparent->data.internal.key_entries +
		   sizeof(BpTreeInternalEntry) * 2 + zeroent->key_len,
	       (u8 *)mident + sizeof(BpTreeEntry), mident->key_len);

	return new;
}

STATIC int split_node(BpTxn *txn, BpTreeNode *node, const void *key,
		      u16 key_len, const void *value, u16 value_len,
		      u16 key_index, u64 space_needed) {
	int i;
	u16 midpoint;
	u16 pos;
	BpTreeNode *new;

	midpoint = node->num_entries / 2;
	new = new_node(txn, node, midpoint);
	if (!new) return -1;

	pos = node->data.leaf.entry_offsets[midpoint];
	memcpy(new->data.leaf.entries, node->data.leaf.entries + pos,
	       node->used_bytes - pos);
	memcpy(node->data.leaf.entry_offsets,
	       new->data.leaf.entry_offsets + midpoint,
	       node->num_entries - midpoint);
	for (i = 0; i < node->num_entries - midpoint; i++)
		new->data.leaf.entry_offsets[i] -= pos;

	if (key_index <= midpoint)
		insert_entry(node, key_index, space_needed, key, key_len, value,
			     value_len);
	else
		insert_entry(new, key_index - midpoint, space_needed, key,
			     key_len, value, value_len);

	return 0;
}

BpTree *bptree_open(const u8 *path) {
	BpTree *ret;
	int fd = file(path);
	u64 capacity;

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
	u64 m1_counter, m2_counter;
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

int bptree_put(BpTxn *txn, const void *key, u16 key_len, const void *value,
	       u32 value_len, const BpTreeSearch search) {
	BpTreeSearchResult res;
	BpTreeNode *node, *copy;
	u64 space_needed;

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
	    (u64)(node->num_entries + 1) >= MAX_LEAF_ENTRIES) {
		if (split_node(txn, node, key, key_len, value, value_len,
			       res.key_index, space_needed) == -1)
			return -1;

	} else {
		copy = allocate_node(txn);
		if (!copy) return -1;
		memcpy(copy, node, NODE_SIZE);
		txn->root = bptree_node_id(txn, copy);

		insert_entry(copy, res.key_index, space_needed, key, key_len,
			     value, value_len);
	}

	return 0;
}

BpTreeEntry *bptree_remove(BpTxn *txn, const void *key, u16 key_len,
			   const void *value, u64 value_len,
			   const BpTreeSearch search);

BpTreeNode *bptxn_get_node(BpTxn *txn, u64 node_id) {
	BpTreeNode *ret = NODE(txn, node_id);
	if ((((u8 *)ret) - ((u8 *)txn->tree->base)) + NODE_SIZE >
	    txn->tree->capacity)
		return NULL;
	return ret;
}
BpTreeNode *bptree_root(BpTxn *txn) {
	BpTreeNode *ret = NODE(txn, txn->root);
	return ret;
}

u64 bptree_node_id(BpTxn *txn, const BpTreeNode *node) {
	return ((u8 *)node - (u8 *)txn->tree->base) / NODE_SIZE;
}
