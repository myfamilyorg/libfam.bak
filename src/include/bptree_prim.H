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
	const void *key;
	const void *value;
} BpKeyValue;

typedef struct {
	u32 value_len;
	u16 key_len;
	bool is_overflow;
	union {
		BpKeyValue kv;
		u64 overflow;
	} vardata;
} BpTreeItem;

i32 bptree_prim_init_node(BpTreeNode *node, u64 parent_id, bool is_internal);
i32 bptree_prim_set_aux(BpTreeNode *node, u64 aux);
i32 bptree_prim_unset_copy(BpTreeNode *node);
i32 bptree_prim_set_leaf_entry(BpTreeNode *node, u16 index, BpTreeItem *item);
i32 bptree_prim_insert_leaf_entry(BpTreeNode *node, u16 index,
				  BpTreeItem *item);
i32 bptree_prim_set_internal_entry(BpTreeNode *node, u16 index,
				   BpTreeItem *item);
i32 bptree_prim_insert_internal_entry(BpTreeNode *node, u16 index,
				      BpTreeItem *item);
i32 bptree_prim_move_leaf_node_entries(BpTreeNode *dst, u16 dst_start_index,
				       BpTreeNode *src, u16 src_start_index,
				       u16 num_entries);
i32 bptree_prim_copy(BpTreeNode *dst, BpTreeNode *src);
u64 bptree_prim_get_parent_id(BpTreeNode *node);
u16 bptree_prim_num_entries(BpTreeNode *node);
u16 bptree_prim_used_bytes(BpTreeNode *node);
u16 bptree_prim_key_len(BpTreeNode *node, u16 index);
u32 bptree_prim_value_len(BpTreeNode *node, u16 index);
const void *bptree_prim_key(BpTreeNode *node, u16 index);
const void *bptree_prim_value(BpTreeNode *node, u16 index);
bool bptree_prim_is_copy(BpTreeNode *node);
bool bptree_prim_is_internal(BpTreeNode *node);
bool bptree_prim_is_copy(BpTreeNode *node);
bool bptree_prim_is_internal(BpTreeNode *node);
u64 bptree_prim_get_aux(BpTreeNode *node);

#endif /* _BPTREE_PRIM_H */
