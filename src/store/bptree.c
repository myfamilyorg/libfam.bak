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
#include <rbtree.H>
#include <sys.H>

#define STATIC_ASSERT(condition, message) \
	typedef u8 static_assert_##message[(condition) ? 1 : -1]

#define MIN_SIZE (NODE_SIZE * 4)
#define BASE_PTR(txn) ((u8 *)txn->tree->base)
#define RUNTIME_NODE(tree) ((RuntimeNode *)((u8 *)tree->base + (NODE_SIZE * 2)))
#define METADATA1(tree) ((MetadataNode *)tree->base)
#define METADATA2(tree) ((MetadataNode *)(((u8 *)tree->base + NODE_SIZE)))
#define NODE_OFFSET(index) (index * NODE_SIZE)
#define NODE(txn, index) \
	((BpTreeNode *)(((u8 *)txn->tree->base) + NODE_SIZE * index))
#define FIRST_NODE 3

STATIC_ASSERT(sizeof(BpTreeNode) == NODE_SIZE, bptree_node_size);
STATIC_ASSERT(sizeof(BpTreeLeafNode) == sizeof(BpTreeInternalNode),
	      bptree_nodes_equal);

struct BpTree {
	void *base;
	u64 capacity;
	i32 fd;
};

struct BpTxn {
	BpTree *tree;
	u64 txn_id;
	u64 root;
	RbTree overrides;
};

typedef struct {
	u64 counter;
	u64 root;
} MetadataNode;

typedef struct {
	u64 next_txn_id;
	u64 free_list_head;
	u64 next_file_node;
} RuntimeNode;

typedef struct {
	RbTreeNode _reserved;
	u64 node_id;
	u64 override;
} BpRbTreeNode;

STATIC i32 bptree_rbtree_search(RbTreeNode *cur, const RbTreeNode *value,
				RbTreeNodePair *retval) {
	while (cur) {
		u64 v1 = ((BpRbTreeNode *)cur)->node_id;
		u64 v2 = ((BpRbTreeNode *)value)->node_id;
		if (v1 == v2) {
			retval->self = cur;
			break;
		} else if (v1 < v2) {
			retval->parent = cur;
			retval->is_right = 1;
			cur = cur->right;
		} else {
			retval->parent = cur;
			retval->is_right = 0;
			cur = cur->left;
		}
		retval->self = cur;
	}
	return 0;
}

STATIC BpTreeNode *allocate_node(BpTxn *txn) {
	RuntimeNode *runtime;
	u64 expected, current;
	if (!txn) return NULL;

	runtime = RUNTIME_NODE(txn->tree);
	do {
		current = ALOAD(&runtime->next_file_node);
		if (current * NODE_SIZE >= txn->tree->capacity) {
			err = ENOMEM;
			return NULL;
		}
		expected = current;
	} while (!__cas64(&runtime->next_file_node, &expected, current + 1));
	return NODE(txn, current);
}

STATIC BpTreeNode *copy_node(BpTxn *txn, BpTreeNode *node) {
	BpTreeNode *copy;
	BpRbTreeNode *value = alloc(sizeof(BpRbTreeNode));
	if (!value) return NULL;
	copy = allocate_node(txn);
	if (!copy) {
		release(value);
		return NULL;
	}

	value->node_id = bptree_node_id(txn, node);
	value->override = bptree_node_id(txn, copy);
	value->_reserved.parent_color = value->_reserved.left =
	    value->_reserved.right = NULL;
	rbtree_put(&txn->overrides, (RbTreeNode *)value, bptree_rbtree_search);
	memcpy(copy, node, NODE_SIZE);
	copy->is_copy = true;

	return copy;
}

STATIC void bptree_ensure_init(BpTree *tree) {
	MetadataNode *m1 = METADATA1(tree);
	RuntimeNode *run = RUNTIME_NODE(tree);
	u64 expected = 0;
	__cas64(&m1->root, &expected, FIRST_NODE);
	expected = 0;
	__cas64(&m1->counter, &expected, 1);
	expected = 0;
	__cas64(&run->next_file_node, &expected, FIRST_NODE + 1);
}

STATIC void shift_by_offset(BpTreeNode *node, u16 key_index, u16 shift) {
	i32 i;
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

STATIC i32 split_node(BpTxn *txn, BpTreeNode *node, const void *key,
		      u16 key_len, const void *value, u16 value_len,
		      u16 key_index, u64 space_needed) {
	BpTreeNode *nparent, *sibling;
	RbTreeNode *dnode;
	BpTreeEntry *ent;
	BpTreeInternalEntry *internal_ent;
	BpRbTreeNode *nvalue;
	u16 midpoint, pos, i;
	if (!txn || !node || !key || key_len == 0 || !value || value_len == 0 ||
	    key_index > node->num_entries || space_needed == 0) {
		return -1;
	}

	if (!node->is_copy) {
		node = copy_node(txn, node);
		if (node == NULL) return -1;
	}

	nparent = allocate_node(txn);
	sibling = allocate_node(txn);

	if (nparent == NULL || sibling == NULL) return -1;
	/* TODO: release any good nodes */

	nparent->is_copy = sibling->is_copy = true;
	nparent->is_internal = true;
	/* TODO: for now just root, but later add support */
	nparent->parent_id = 0;
	nparent->num_entries = 2;
	sibling->parent_id = bptree_node_id(txn, nparent);
	sibling->is_internal = false;

	midpoint = node->num_entries / 2; /* TODO: edge cases */
	pos = node->data.leaf.entry_offsets[midpoint];

	memcpy(sibling->data.leaf.entries, node->data.leaf.entries + pos,
	       node->used_bytes - pos);
	memcpy(sibling->data.leaf.entry_offsets,
	       node->data.leaf.entry_offsets + midpoint,
	       (node->num_entries - midpoint) * sizeof(u16));
	for (i = 0; i < node->num_entries - midpoint; i++) {
		sibling->data.leaf.entry_offsets[i] -= pos;
	}
	sibling->num_entries = node->num_entries - midpoint;
	node->num_entries = midpoint;
	sibling->used_bytes = node->used_bytes - pos;
	node->used_bytes = pos;

	nparent->data.internal.entry_offsets[0] = 0;
	ent = (BpTreeEntry *)node->data.leaf.entries;
	internal_ent =
	    (BpTreeInternalEntry *)nparent->data.internal.key_entries;
	internal_ent->node_id = bptree_node_id(txn, node);
	internal_ent->key_len = ent->key_len;

	memcpy((u8 *)nparent->data.internal.key_entries +
		   sizeof(BpTreeInternalEntry),
	       node->data.leaf.entries + sizeof(BpTreeEntry), ent->key_len);
	nparent->data.internal.entry_offsets[1] =
	    ent->key_len + sizeof(BpTreeInternalEntry);
	ent = (BpTreeEntry *)sibling->data.leaf.entries;
	internal_ent =
	    (BpTreeInternalEntry *)((u8 *)nparent->data.internal.key_entries +
				    nparent->data.internal.entry_offsets[1]);
	internal_ent->node_id = bptree_node_id(txn, sibling);
	internal_ent->key_len = ent->key_len;
	memcpy((u8 *)nparent->data.internal.key_entries +
		   nparent->data.internal.entry_offsets[1] +
		   sizeof(BpTreeInternalEntry),
	       sibling->data.leaf.entries + sizeof(BpTreeEntry), ent->key_len);

	nparent->used_bytes = nparent->data.internal.entry_offsets[1] +
			      ent->key_len + sizeof(BpTreeInternalEntry);

	nvalue = alloc(sizeof(BpRbTreeNode));
	if (nvalue == NULL) {
		/* TODO: cleanup */
		return -1;
	}
	nvalue->node_id = txn->root;
	nvalue->override = bptree_node_id(txn, nparent);
	rbtree_init_node((RbTreeNode *)nvalue);

	/* TODO release returned node */
	dnode = rbtree_put(&txn->overrides, (RbTreeNode *)nvalue,
			   bptree_rbtree_search);
	if (dnode) release(dnode);

	if (key_index <= midpoint)
		insert_entry(node, key_index, space_needed, key, key_len, value,
			     value_len);
	else
		insert_entry(sibling, key_index - midpoint, space_needed, key,
			     key_len, value, value_len);

	return 0;
}

BpTree *bptree_open(const u8 *path) {
	BpTree *ret;
	i32 fd = file(path);
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

i32 bptree_close(BpTree *tree) {
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
	ret->txn_id = __add64(&(RUNTIME_NODE(tree)->next_txn_id), 1);
	ret->overrides = INIT_RBTREE;

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

i32 bptxn_abort(BpTxn *txn) {
	BpRbTreeNode *node;

	while ((node = (BpRbTreeNode *)txn->overrides.root)) {
		BpRbTreeNode *to_del = (BpRbTreeNode *)rbtree_remove(
		    &txn->overrides, (RbTreeNode *)node, bptree_rbtree_search);
		release(to_del);
	}

	return 0;
}

BpTreeNode *bptxn_get_node(BpTxn *txn, u64 node_id) {
	RbTreeNodePair retval = {0};
	BpRbTreeNode value;
	BpTreeNode *ret = NODE(txn, node_id);
	if ((((u8 *)ret) - ((u8 *)txn->tree->base)) + NODE_SIZE >
	    txn->tree->capacity)
		return NULL;
	/* Check for overrides */
	value.node_id = node_id;
	bptree_rbtree_search(txn->overrides.root, (RbTreeNode *)&value,
			     &retval);

	if (retval.self) {
		u64 override = ((BpRbTreeNode *)retval.self)->override;
		return NODE(txn, override);
	}

	return ret;
}
BpTreeNode *bptree_root(BpTxn *txn) {
	BpTreeNode *ret = bptxn_get_node(txn, txn->root);
	return ret;
}

u64 bptree_node_id(BpTxn *txn, const BpTreeNode *node) {
	return ((u8 *)node - (u8 *)txn->tree->base) / NODE_SIZE;
}

i32 bptree_put(BpTxn *txn, const void *key, u16 key_len, const void *value,
	       u32 value_len, const BpTreeSearch search) {
	BpTreeSearchResult res;
	BpTreeNode *node;
	u64 space_needed;

	if (key_len == 0 || value_len == 0 || key == NULL || value == NULL ||
	    txn == NULL) {
		err = EINVAL;
		return -1;
	}

	{
		u8 tmp[1024] = {0};
		memcpy(tmp, key, key_len);
		tmp[key_len] = 0;
	}

	node = bptxn_get_node(txn, txn->root);
	search(txn, key, key_len, node, &res);
	if (res.found) return -1;
	node = bptxn_get_node(txn, res.node_id);

	space_needed = key_len + value_len + sizeof(BpTreeEntry);
	if (space_needed + node->used_bytes > LEAF_ARRAY_SIZE ||
	    (u64)(node->num_entries + 1) >= MAX_LEAF_ENTRIES) {
		split_node(txn, node, key, key_len, value, value_len,
			   res.key_index, space_needed);

	} else {
		if (!node->is_copy) {
			node = copy_node(txn, node);
			if (node == NULL) return -1;
		}

		insert_entry(node, res.key_index, space_needed, key, key_len,
			     value, value_len);
	}

	return 0;
}
