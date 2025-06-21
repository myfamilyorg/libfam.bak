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

#ifndef _BPTREE_PRIM_H
#define _BPTREE_PRIM_H

#include <types.H>

#ifndef NODE_SIZE
#ifndef PAGE_SIZE
#define PAGE_SIZE (4 * 4096)
#endif /* PAGE_SIZE */
#define NODE_SIZE ((u64)PAGE_SIZE)
#endif /* NODE_SIZE */

/* BpTree Node */
typedef struct {
	u8 data[NODE_SIZE];
} BpTreeNode;

typedef struct {
	const void *value;
	u32 value_len;
} BpKeyValue;

typedef struct {
	u64 overflow_start;
	u64 overflow_end;
	u32 value_len;
} BpOverflow;

typedef struct {
	u64 node_id;
} BpInternal;

typedef enum {
	BPTREE_ITEM_TYPE_LEAF,
	BPTREE_ITEM_TYPE_OVERFLOW,
	BPTREE_ITEM_TYPE_INTERNAL
} BpTreeItemType;

typedef struct {
	u16 key_len;
	const void *key;
	BpTreeItemType item_type;
	union {
		BpKeyValue kv;
		BpOverflow overflow;
		BpInternal internal;
	} vardata;
} BpTreeItem;

/* Create/Modify node */
i32 bptree_prim_init_node(BpTreeNode *node, u64 parent_id, bool is_internal);
i32 bptree_prim_set_aux(BpTreeNode *node, u64 aux);
i32 bptree_prim_unset_copy(BpTreeNode *node);
i32 bptree_prim_copy(BpTreeNode *dst, BpTreeNode *src);
i32 bptree_prim_set_next_leaf(BpTreeNode *node, u64 next_leaf);

/* Insert */
i32 bptree_prim_insert_entry(BpTreeNode *node, u16 index, BpTreeItem *item);
/* Update */
i32 bptree_prim_set_entry(BpTreeNode *node, u16 index, BpTreeItem *item);
/* Delete */
i32 bptree_prim_delete_entry(BpTreeNode *node, u16 index);
/* Move */
i32 bptree_prim_move_entries(BpTreeNode *dst, u16 dst_start_index,
			     BpTreeNode *src, u16 src_start_index,
			     u16 num_entries);
/* Accessors */
u64 bptree_prim_parent_id(BpTreeNode *node);
u16 bptree_prim_num_entries(BpTreeNode *node);
u16 bptree_prim_used_bytes(BpTreeNode *node);
bool bptree_prim_is_copy(BpTreeNode *node);
bool bptree_prim_is_internal(BpTreeNode *node);
u64 bptree_prim_aux(BpTreeNode *node);

int bptree_prim_is_overflow(BpTreeNode *node, u16 index);
u16 bptree_prim_key_len(BpTreeNode *node, u16 index);
u32 bptree_prim_value_len(BpTreeNode *node, u16 index);
u64 bptree_prim_overflow_start(BpTreeNode *node, u16 index);
u64 bptree_prim_overflow_end(BpTreeNode *node, u16 index);
const void *bptree_prim_key(BpTreeNode *node, u16 index);
const void *bptree_prim_value(BpTreeNode *node, u16 index);
u64 bptree_prim_next_leaf(BpTreeNode *node);
u64 bptree_prim_node_id(BpTreeNode *node, u16 index);

#endif /* _BPTREE_PRIM_H */
