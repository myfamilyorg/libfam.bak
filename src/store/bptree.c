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
#include <storage.H>

#define NODE(txn, index) \
	((BpTreeNode *)(((u8 *)env_base(txn->tree->env)) + NODE_SIZE * index))

struct BpTree {
	Env *env;
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

STATIC void release_node(BpTxn *txn, BpTreeNode *node) {
	u64 node_id = bptree_node_id(txn, node);
	env_release(txn->tree->env, node_id);
}

STATIC BpTreeNode *allocate_node(BpTxn *txn) {
	u64 index = env_alloc(txn->tree->env);
	if (index == 0) return NULL;
	return (BpTreeNode *)((u8 *)env_base(txn->tree->env) +
			      index * NODE_SIZE);
}

STATIC BpTreeNode *copy_node(BpTxn *txn, BpTreeNode *node) {
	BpTreeNode *ret = NULL;
	BpRbTreeNode *value;

	ret = allocate_node(txn);
	if (!ret) return NULL;

	value = alloc(sizeof(BpRbTreeNode));
	if (!value) {
		release_node(txn, ret);
		return NULL;
	}

	bptree_prim_copy(ret, node);

	rbtree_init_node((RbTreeNode *)value);
	value->node_id = bptree_node_id(txn, node);
	value->override = bptree_node_id(txn, ret);
	rbtree_put(&txn->overrides, (RbTreeNode *)value, bptree_rbtree_search);

	return ret;
}

i32 bptree_put(BpTxn *txn, const void *key, u16 key_len, const void *value,
	       u32 value_len, const BpTreeSearch search) {
	BpTreeSearchResult res;
	BpTreeNode *node;
	BpTreeItem item;

	if (!txn || !key || !value || key_len == 0 || value_len == 0) {
		err = EINVAL;
		return -1;
	}

	node = bptree_root(txn);
	search(txn, key, key_len, node, &res);
	node = bptxn_get_node(txn, res.node_id);
	if (!bptree_prim_is_copy(node)) {
		node = copy_node(txn, node);
	}

	item.key_len = key_len;
	item.item_type = BPTREE_ITEM_TYPE_LEAF;
	item.vardata.kv.value_len = value_len;
	item.key = key;
	item.vardata.kv.value = value;

	if (bptree_prim_insert_entry(node, res.key_index, &item) == 0) return 0;

	return -1;
}

BpTree *bptree_open(Env *env) {
	u64 eroot, seqno;
	BpTree *ret = alloc(sizeof(BpTree));
	if (!ret) return NULL;

	seqno = env_root_seqno(env);
	eroot = env_root(env);

	if (!eroot) {
		BpTreeNode *node;
		eroot = env_alloc(env);
		if (!eroot || env_set_root(env, seqno, eroot) < 0) {
			if (eroot) env_release(env, eroot);
			release(ret);
			return NULL;
		}
		node =
		    (BpTreeNode *)(((u8 *)env_base(env)) + eroot * NODE_SIZE);
		bptree_prim_unset_copy(node);
	}

	ret->env = env;
	return ret;
}

BpTxn *bptxn_start(BpTree *tree) {
	BpTxn *ret = alloc(sizeof(BpTxn));
	if (ret == NULL) return NULL;
	ret->tree = tree;
	/*ret->txn_id = __add64(&tree->meta->next_bptxn_id, 1);*/
	ret->overrides = INIT_RBTREE;
	ret->seqno = env_root_seqno(tree->env);
	ret->root = env_root(tree->env);
	return ret;
}

void bptxn_abort(BpTxn *txn) {
	BpRbTreeNode *node;

	while ((node = (BpRbTreeNode *)txn->overrides.root)) {
		BpRbTreeNode *to_del = (BpRbTreeNode *)rbtree_remove(
		    &txn->overrides, (RbTreeNode *)node, bptree_rbtree_search);
		release(to_del);
	}
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

