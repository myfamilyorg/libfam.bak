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
#include <limits.H>
#include <misc.H>
#include <rbtree.H>
#include <storage.H>

#define NODE(txn, index) \
	((BpTreeNode *)(((u8 *)env_base(txn->tree->env)) + NODE_SIZE * index))

struct BpTree {
	Env *env;
	u64 meta;
	MetaData *meta_ptr;
};

struct BpTxn {
	BpTree *tree;
	u64 txn_id;
	u64 root;
	u64 seqno;
	u64 counter;
	RbTree overrides;
};

typedef struct {
	RbTreeNode _reserved;
	u64 node_id;
	u64 override;
} BpRbTreeNode;

typedef void (*RbTreeNodeCallback)(BpRbTreeNode *node, void *user_data);

STATIC i32 bptree_rbtree_dfs(RbTreeNode *cur, RbTreeNodeCallback callback,
			     void *user_data) {
	i32 status;
	if (!cur) {
		return 0;
	}

	status = bptree_rbtree_dfs(cur->left, callback, user_data);
	if (status != 0) {
		return status;
	}

	callback((BpRbTreeNode *)cur, user_data);

	status = bptree_rbtree_dfs(cur->right, callback, user_data);

	release(cur);
	if (status != 0) {
		return status;
	}

	return 0;
}

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

STATIC BpTreeNode *copy_node(BpTxn *txn, BpTreeNode *node,
			     BpTreeSearchResult *res) {
	BpTreeNode *ret = NULL, *next = NULL, *last = NULL;
	BpRbTreeNode *value;
	u64 parent_id;
	u16 level;

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

	last = next = ret;
	level = 0;
	while ((parent_id = bptree_prim_parent_id(next))) {
		u8 buf[NODE_SIZE];
		BpTreeItem item;
		u16 index;
		u64 copy_id = bptree_node_id(txn, next);
		BpTreeNode *nnode;
		level++;
		next = bptxn_get_node(txn, parent_id);
		index = res->parent_index[res->levels - level];
		value = alloc(sizeof(BpRbTreeNode));
		if (!value) {
			/* TODO: cleanup */
			return NULL;
		}

		nnode = allocate_node(txn);
		if (!nnode) {
			/* TODO: cleanup */
			return NULL;
		}
		bptree_prim_copy(nnode, next);

		item.key_len = bptree_prim_key_len(nnode, index);
		item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
		item.key = buf;
		memcpy(buf, bptree_prim_key(nnode, index), item.key_len);
		item.vardata.internal.node_id = copy_id;

		bptree_prim_set_entry(nnode, index, &item);

		rbtree_init_node((RbTreeNode *)value);
		value->node_id = bptree_node_id(txn, next);
		value->override = bptree_node_id(txn, nnode);
		rbtree_put(&txn->overrides, (RbTreeNode *)value,
			   bptree_rbtree_search);
		bptree_prim_set_parent(last, parent_id);
		last = next;
	}

	return ret;
}

STATIC i32 try_merge(BpTxn *txn, BpTreeSearchResult *res, BpTreeNode *node,
		     const void *key, u16 key_len, const void *value,
		     u16 value_len) {
	/* TODO: unimplemneted */
	if (!txn || !node || !key || key_len == 0 || !value || value_len == 0 ||
	    !res) {
		return -1;
	}

	return -1;
}

STATIC i32 bptree_split(BpTxn *txn, BpTreeSearchResult *res, BpTreeNode *node,
			const void *key, u16 key_len, const void *value,
			u32 value_len) {
	BpTreeNode *parent = NULL, *sibling = NULL;
	BpTreeItem item, to_insert;
	u64 parent_id;
	u16 partition_point, num_entries, used_left, used_right;
	BpRbTreeNode *rbvalue;

	if (!txn || !res || !key || !value || key_len == 0 || value_len == 0) {
		err = EINVAL;
		return -1;
	}

	/* Try to merge if we can */
	if (try_merge(txn, res, node, key, key_len, value, value_len) == 0)
		return 0;

	if ((parent_id = bptree_prim_parent_id(node))) {
		parent = bptxn_get_node(txn, parent_id);
	} else {
		BpRbTreeNode *overwrite;
		rbvalue = alloc(sizeof(BpRbTreeNode));
		if (!rbvalue) return -1;
		parent = allocate_node(txn);
		if (!parent) {
			release(rbvalue);
			return -1;
		}
		bptree_prim_set_aux(parent, bptree_prim_aux(bptree_root(txn)));
		parent_id = bptree_node_id(txn, parent);
		bptree_prim_init_node(parent, 0, true);
		bptree_prim_set_parent(node, parent_id);
		rbtree_init_node((RbTreeNode *)rbvalue);
		rbvalue->override = bptree_node_id(txn, parent);
		rbvalue->node_id = txn->root;
		overwrite = (BpRbTreeNode *)rbtree_put(&txn->overrides,
						       (RbTreeNode *)rbvalue,
						       bptree_rbtree_search);
		if (overwrite) {
			overwrite->node_id = txn->counter--;
			rbtree_put(&txn->overrides, (RbTreeNode *)overwrite,
				   bptree_rbtree_search);
		}
	}

	sibling = allocate_node(txn);
	if (!sibling || !parent) {
		/* TODO: memory cleanup */
		return -1;
	}

	rbvalue = alloc(sizeof(BpRbTreeNode));
	if (!rbvalue) return -1; /* TODO: memory cleanup */

	rbvalue->override = bptree_node_id(txn, sibling);
	rbvalue->node_id = txn->counter--;
	rbtree_put(&txn->overrides, (RbTreeNode *)rbvalue,
		   bptree_rbtree_search);

	bptree_prim_init_node(sibling, parent_id, false);
	num_entries = bptree_prim_num_entries(node);
	partition_point = (num_entries + 1) / 2;
	to_insert.key_len = key_len;
	to_insert.item_type = BPTREE_ITEM_TYPE_LEAF;
	to_insert.vardata.kv.value_len = value_len;
	to_insert.key = key;
	to_insert.vardata.kv.value = value;

	do {
		u16 needed = calculate_needed(node, &to_insert);
		used_left = bptree_prim_offset(node, partition_point);
		used_right = bptree_prim_used_bytes(node) - used_left;
		if (res->key_index > partition_point)
			used_right += needed;
		else
			used_left += needed;

		if (used_right > LEAF_ARRAY_SIZE) {
			partition_point++;
		}
		if (used_left > LEAF_ARRAY_SIZE) {
			partition_point--;
		}
	} while (used_right > LEAF_ARRAY_SIZE || used_left > LEAF_ARRAY_SIZE);

	bptree_prim_move_entries(sibling, 0, node, partition_point,
				 num_entries - partition_point);

	if (res->levels == 0) {
		item.key_len = bptree_prim_key_len(node, 0);
		item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
		item.key = bptree_prim_key(node, 0);
		item.vardata.internal.node_id = bptree_node_id(txn, node);
		bptree_prim_insert_entry(parent, 0, &item);

		item.key_len = bptree_prim_key_len(sibling, 0);
		item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
		item.key = bptree_prim_key(sibling, 0);
		item.vardata.internal.node_id = bptree_node_id(txn, sibling);
		bptree_prim_insert_entry(parent, 1, &item);
	} else {
		u16 index = res->parent_index[res->levels - 1] + 1;
		item.key_len = bptree_prim_key_len(sibling, 0);
		item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
		item.key = bptree_prim_key(sibling, 0);
		item.vardata.internal.node_id = bptree_node_id(txn, sibling);
		bptree_prim_insert_entry(parent, index, &item);
	}

	if (res->key_index > partition_point) {
		bptree_prim_insert_entry(
		    sibling, res->key_index - partition_point, &to_insert);
	} else {
		bptree_prim_insert_entry(node, res->key_index, &to_insert);
	}

	return 0;
}

STATIC void do_callback(BpRbTreeNode *rbnode, void *user_data) {
	BpTxn *txn = user_data;
	BpTreeNode *node = bptxn_get_node(txn, rbnode->node_id);
	bptree_prim_unset_copy(node);
}

i32 bptree_put(BpTxn *txn, const void *key, u16 key_len, const void *value,
	       u32 value_len, const BpTreeSearch search) {
	BpTreeSearchResult res;
	BpTreeNode *node;
	BpTreeItem item;

	if (!txn || !key || !value || key_len == 0 || value_len == 0 ||
	    key_len > (INTERNAL_ARRAY_SIZE / 2)) {
		err = EINVAL;
		return -1;
	}

#ifdef DEBUG1
	{
		u8 tmp[NODE_SIZE];
		memcpy(tmp, key, key_len);
		tmp[key_len] = 0;
		printf("put '%s'\n", tmp);
	}
#endif /* DEBUG1 */

	node = bptree_root(txn);
	search(txn, key, key_len, node, &res);
	if (res.found) {
		err = EKEYREJECTED;
		return -1;
	}
	node = bptxn_get_node(txn, res.node_id);
	if (!bptree_prim_is_copy(node)) node = copy_node(txn, node, &res);

	item.key_len = key_len;
	item.item_type = BPTREE_ITEM_TYPE_LEAF;
	item.vardata.kv.value_len = value_len;
	item.key = key;
	item.vardata.kv.value = value;

	if (bptree_prim_insert_entry(node, res.key_index, &item) == 0) return 0;

	return bptree_split(txn, &res, node, key, key_len, value, value_len);
}

BpTree *bptree_open(Env *env) {
	u64 eroot, meta, seqno;
	BpTree *ret = alloc(sizeof(BpTree));
	if (!ret) return NULL;

	seqno = env_root_seqno(env);
	eroot = env_root(env);

	if (!eroot) {
		BpTreeNode *node;
		eroot = env_alloc(env);
		meta = env_alloc(env);
		if (!eroot || !meta || env_set_root(env, seqno, eroot) < 0) {
			if (eroot) env_release(env, eroot);
			if (meta) env_release(env, meta);
			release(ret);
			return NULL;
		}
		node =
		    (BpTreeNode *)(((u8 *)env_base(env)) + eroot * NODE_SIZE);
		bptree_prim_set_aux(node, meta);
		bptree_prim_unset_copy(node);
	} else {
		BpTreeNode *node =
		    (BpTreeNode *)((u8 *)env_base(env) + NODE_SIZE * eroot);
		meta = bptree_prim_aux(node);
	}

	ret->meta = meta;
	ret->meta_ptr = (MetaData *)((u8 *)env_base(env) + NODE_SIZE * meta);
	ret->env = env;
	return ret;
}

BpTxn *bptxn_start(BpTree *tree) {
	BpTxn *ret = alloc(sizeof(BpTxn));
	if (ret == NULL) return NULL;
	ret->tree = tree;
	ret->txn_id = __add64(&tree->meta_ptr->next_bptxn_id, 1);
	ret->overrides = INIT_RBTREE;
	ret->seqno = env_root_seqno(tree->env);
	ret->counter = UINT64_MAX;
	ret->root = env_root(tree->env);
	return ret;
}

i64 bptxn_commit(BpTxn *txn, i32 wakeupfd) {
	u64 root = bptree_node_id(txn, bptree_root(txn));
	u64 envroot = env_root(txn->tree->env);

	bptree_rbtree_dfs(txn->overrides.root, do_callback, txn);

	if (root == envroot) return 0;
	if (env_set_root(txn->tree->env, txn->seqno, root) < 0) return -1;
	if (wakeupfd >= 0)
		return env_register_notification(txn->tree->env, wakeupfd);
	return 0;
}

void bptxn_abort(BpTxn *txn) {
	BpRbTreeNode *node;

	while ((node = (BpRbTreeNode *)txn->overrides.root)) {
		BpRbTreeNode *to_del = (BpRbTreeNode *)rbtree_remove(
		    &txn->overrides, (RbTreeNode *)node, bptree_rbtree_search);
		if (to_del) {
		}
		/*env_release(txn->tree->env, id);*/
		release(to_del);
	}
}

u64 bptxn_id(BpTxn *txn) { return txn->txn_id; }

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

