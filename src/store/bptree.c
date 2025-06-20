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

/*
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
	u64 seqno;
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
	BpRbTreeNode *value;

	if (node->is_copy) return node;

	value = alloc(sizeof(BpRbTreeNode));
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

STATIC BpTreeNode *copy_path(BpTxn *txn, BpTreeNode *node) {
	return copy_node(txn, node);
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

	node = bptxn_get_node(txn, res.node_id);
	space_needed = key_len + value_len + sizeof(BpTreeEntry);

	if (space_needed) {
	}

	node = copy_path(txn, node);
	if (!node) return -1;

	return 0;
}

BpTree *bptree_open(Env *env) {
	BpTreeNode *root;
	u64 eroot, meta, seqno;

	BpTree *ret = alloc(sizeof(BpTree));
	if (!ret) return NULL;

	seqno = env_root_seqno(env);
	eroot = env_root(env);

	if (!eroot) {
		meta = env_alloc(env);
		eroot = env_alloc(env);
		if (!eroot || !meta || env_set_root(env, seqno, eroot) < 0) {
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
*/

int x;
