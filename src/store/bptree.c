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

#define STATIC_ASSERT(condition, message) \
	typedef u8 static_assert_##message[(condition) ? 1 : -1]

#define NODE(txn, index) \
	((BpTreeNode *)(((u8 *)env_base(txn->tree->env)) + NODE_SIZE * index))

STATIC_ASSERT(sizeof(BpTreeNode) == NODE_SIZE, bptree_node_size);
STATIC_ASSERT(sizeof(BpTreeLeafNode) == sizeof(BpTreeInternalNode),
	      bptree_nodes_equal);

struct BpTree {
	Env *env;
	MetaData *meta;
};

struct BpTxn {
	BpTree *tree;
	u64 txn_id;
	u64 root;
	RbTree overrides;
};

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
	u64 index = env_alloc(txn->tree->env);
	if (index == 0) return NULL;
	return (BpTreeNode *)((u8 *)env_base(txn->tree->env) +
			      index * NODE_SIZE);
}

STATIC void release_node(BpTxn *txn, BpTreeNode *node) {
	u64 node_id = bptree_node_id(txn, node);
	env_release(txn->tree->env, node_id);
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

	rbtree_init_node((RbTreeNode *)value);
	value->node_id = bptree_node_id(txn, node);
	value->override = bptree_node_id(txn, copy);
	rbtree_put(&txn->overrides, (RbTreeNode *)value, bptree_rbtree_search);
	memcpy(copy, node, NODE_SIZE);
	copy->is_copy = true;

	return copy;
}

STATIC i32 try_merge(BpTxn *txn, BpTreeNode *node, const void *key, u16 key_len,
		     const void *value, u16 value_len, BpTreeSearchResult *res,
		     u64 space_needed) {
	/* TODO: unimplemneted */
	if (!txn || !node || !key || key_len == 0 || !value || value_len == 0 ||
	    !res || space_needed == 0) {
		return -1;
	}

	return -1;
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

STATIC void shift_by_offset_internal(BpTreeNode *node, u16 key_index,
				     u16 shift) {
	i32 i;
	u16 pos = node->data.internal.entry_offsets[key_index];
	u16 bytes_to_move = node->used_bytes - pos;
	void *dst = node->data.internal.entries + pos + shift;
	void *src = node->data.internal.entries + pos;

	memorymove(dst, src, bytes_to_move);
	for (i = node->num_entries; i > key_index; i--) {
		node->data.internal.entry_offsets[i] =
		    node->data.internal.entry_offsets[i - 1] + shift;
	}
}

STATIC void update_nodes(BpTxn *txn, BpTreeNode *node, BpTreeNode *parent,
			 BpTreeNode *sibling, BpRbTreeNode *nvalue,
			 const void *key, u16 key_len, const void *value,
			 u32 value_len, BpTreeSearchResult *res,
			 u64 space_needed) {
	u16 midpoint, pos, i, offset;
	RbTreeNode *dnode;
	BpTreeEntry *ent;
	BpTreeInternalEntry *internal_ent;

	/* Update the sibling */
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
	node->parent_id = bptree_node_id(txn, parent);
	sibling->parent_id = bptree_node_id(txn, parent);
	sibling->is_internal = false;
	sibling->is_copy = true;

	if (res->key_index <= midpoint)
		insert_entry(node, res->key_index, space_needed, key, key_len,
			     value, value_len);
	else
		insert_entry(sibling, res->key_index - midpoint, space_needed,
			     key, key_len, value, value_len);

	/* TODO: for now just root, but later add support */
	internal_ent =
	    (BpTreeInternalEntry *)((u8 *)parent->data.internal.entries);

	offset = 1;
	if (res->levels) offset += res->parent_index[0];

	if (offset < parent->num_entries) {
		BpTreeInternalEntry *partition =
		    (BpTreeInternalEntry
			 *)((u8 *)parent->data.internal.entries +
			    parent->data.internal.entry_offsets[offset]);
		u16 shift = sizeof(BpTreeInternalEntry) + partition->key_len;
		shift_by_offset_internal(parent, offset, shift);
	}

	parent->num_entries++;

	parent->data.internal.entry_offsets[offset] =
	    parent->data.internal.entry_offsets[offset - 1] +
	    internal_ent->key_len + sizeof(BpTreeInternalEntry);
	ent = (BpTreeEntry *)sibling->data.leaf.entries;
	internal_ent =
	    (BpTreeInternalEntry *)((u8 *)parent->data.internal.entries +
				    parent->data.internal
					.entry_offsets[offset]);
	internal_ent->node_id = bptree_node_id(txn, sibling);
	internal_ent->key_len = ent->key_len;
	memcpy((u8 *)parent->data.internal.entries +
		   parent->data.internal.entry_offsets[offset] +
		   sizeof(BpTreeInternalEntry),
	       sibling->data.leaf.entries + sizeof(BpTreeEntry), ent->key_len);

	parent->used_bytes = parent->data.internal.entry_offsets[offset] +
			     ent->key_len + sizeof(BpTreeInternalEntry);

	nvalue->node_id = txn->root;
	nvalue->override = bptree_node_id(txn, parent);
	rbtree_init_node((RbTreeNode *)nvalue);

	dnode = rbtree_put(&txn->overrides, (RbTreeNode *)nvalue,
			   bptree_rbtree_search);
	if (dnode) release(dnode);
}

STATIC i32 split_node(BpTxn *txn, BpTreeNode *node, const void *key,
		      u16 key_len, const void *value, u16 value_len,
		      BpTreeSearchResult *res, u64 space_needed) {
	BpTreeNode *parent, *sibling;
	BpRbTreeNode *nvalue;

	if (!txn || !node || !key || key_len == 0 || !value || value_len == 0 ||
	    !res || space_needed == 0) {
		return -1;
	}

	/* We were able to merge so return */
	if (try_merge(txn, node, key, key_len, value, value_len, res,
		      space_needed) == 0)
		return 0;

	nvalue = alloc(sizeof(BpRbTreeNode));
	if (!nvalue) return -1;

	if (node->parent_id) {
		parent = bptxn_get_node(txn, node->parent_id);
	} else {
		BpTreeEntry *ent = (BpTreeEntry *)node->data.leaf.entries;
		BpTreeInternalEntry ient;
		parent = allocate_node(txn);
		if (!parent) return -1;

		parent->meta = txn->tree->meta;
		parent->is_copy = true;
		parent->is_internal = true;
		parent->parent_id = 0;
		parent->num_entries = 1;
		parent->used_bytes = ent->key_len + sizeof(BpTreeInternalEntry);
		parent->data.internal.entry_offsets[0] = 0;
		ient.key_len = ent->key_len;
		ient.node_id = bptree_node_id(txn, node);
		memcpy((u8 *)parent->data.internal.entries, (u8 *)&ient,
		       sizeof(BpTreeInternalEntry));
		memcpy((u8 *)parent->data.internal.entries +
			   sizeof(BpTreeInternalEntry),
		       (u8 *)node->data.leaf.entries + sizeof(BpTreeEntry),
		       ient.key_len);
	}
	sibling = allocate_node(txn);

	if (!sibling) {
		if (!node->parent_id) release_node(txn, parent);
		release(nvalue);
		return -1;
	}

	update_nodes(txn, node, parent, sibling, nvalue, key, key_len, value,
		     value_len, res, space_needed);

	return 0;
}

i32 bptree_put(BpTxn *txn, const void *key, u16 key_len, const void *value,
	       u32 value_len, const BpTreeSearch search) {
	BpTreeSearchResult res;
	BpTreeNode *node;
	u64 space_needed;

	if (!txn || !key || !value || key_len == 0 || value_len == 0) {
		err = EINVAL;
		return -1;
	}

	node = bptree_root(txn);
	search(txn, key, key_len, node, &res);
	if (res.found) return -1;

	{
		char tmp[NODE_SIZE];
		memcpy(tmp, key, key_len);
		tmp[key_len] = 0;
		/*printf("key=%s\n", tmp);*/
	}

	node = bptxn_get_node(txn, res.node_id);
	space_needed = key_len + value_len + sizeof(BpTreeEntry);

	if (!node->is_copy) {
		node = copy_node(txn, node);
		if (node == NULL) return -1;
	}

	if (space_needed + node->used_bytes > LEAF_ARRAY_SIZE ||
	    (u64)(node->num_entries + 1) >= MAX_LEAF_ENTRIES) {
		split_node(txn, node, key, key_len, value, value_len, &res,
			   space_needed);
	} else {
		insert_entry(node, res.key_index, space_needed, key, key_len,
			     value, value_len);
	}

	return 0;
}

BpTree *bptree_open(Env *env) {
	BpTreeNode *root;
	u64 eroot, meta;
	BpTree *ret = alloc(sizeof(BpTree));
	if (!ret) return NULL;

	eroot = env_root(env);
	if (!eroot) {
		meta = env_alloc(env);
		eroot = env_alloc(env);
		if (!eroot || !meta || env_set_root(env, eroot) < 0) {
			if (eroot) env_release(env, eroot);
			if (meta) env_release(env, meta);
			release(ret);
			return NULL;
		}
		root = (BpTreeNode *)((u8 *)env_base(env) + eroot * NODE_SIZE);
		root->meta =
		    (MetaData *)((u8 *)env_base(env) + meta * NODE_SIZE);
	}

	ret->env = env;
	root = (BpTreeNode *)((u8 *)env_base(env) + eroot * NODE_SIZE);
	ret->meta = root->meta;

	return ret;
}

BpTxn *bptxn_start(BpTree *tree) {
	BpTxn *ret = alloc(sizeof(BpTxn));
	if (ret == NULL) return NULL;
	ret->tree = tree;
	ret->txn_id = __add64(&tree->meta->next_bptxn_id, 1);
	ret->overrides = INIT_RBTREE;
	ret->root = env_root(tree->env);
	return ret;
}

i64 bptxn_commit(BpTxn *txn, int wakeupfd) {
	u64 root = bptree_node_id(txn, bptree_root(txn));
	u64 envroot = env_root(txn->tree->env);

	if (root == envroot) return bptxn_abort(txn);
	env_set_root(txn->tree->env, root);
	if (wakeupfd > 0) {
		bptxn_abort(txn);
		return env_register_notification(txn->tree->env, wakeupfd);
	} else
		return 0;
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
	return ((u8 *)node - (u8 *)((u64)env_base(txn->tree->env))) / NODE_SIZE;
}

