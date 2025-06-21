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

#include <bptree_prim.H>
#include <error.H>
#include <format.H>
#include <misc.H>

#define MAX_LEAF_ENTRIES ((u64)(NODE_SIZE / 32))
#define MAX_INTERNAL_ENTRIES (NODE_SIZE / 16)
#define INTERNAL_ARRAY_SIZE ((28 * NODE_SIZE - 1024) / 32)
#define LEAF_ARRAY_SIZE ((u64)((30 * NODE_SIZE - 1280) / 32))

/* Data specific to an internal node */
typedef struct {
	u16 entry_offsets[MAX_INTERNAL_ENTRIES];
	u8 entries[INTERNAL_ARRAY_SIZE];
} BpTreeInternalNode;

/* Data specific to a leaf node */
typedef struct {
	u64 next_leaf;
	u16 entry_offsets[MAX_LEAF_ENTRIES];
	u8 entries[LEAF_ARRAY_SIZE];
} BpTreeLeafNode;

typedef struct {
	u64 aux;
	u64 parent_id;
	u64 reserved;
	u16 num_entries;
	u16 used_bytes;
	bool is_internal;
	bool is_copy;
	union {
		BpTreeInternalNode internal;
		BpTreeLeafNode leaf;
	} data;
} BpTreeNodeImpl;

/* BpTree entry */
typedef struct {
	u32 value_len;
	u16 key_len;
	bool overflow;
} BpTreeLeafEntry;

/* BpTree Internal entry */
typedef struct {
	u64 node_id;
	u16 key_len;
} BpTreeInternalEntry;

#define STATIC_ASSERT(condition, message) \
	typedef u8 static_assert_##message[(condition) ? 1 : -1]

STATIC_ASSERT(sizeof(BpTreeNode) == NODE_SIZE, bptree_node_size);
STATIC_ASSERT(sizeof(BpTreeNodeImpl) == NODE_SIZE, bptreeimpl_node_size);
STATIC_ASSERT(sizeof(BpTreeLeafNode) == sizeof(BpTreeInternalNode),
	      bptree_nodes_equal);

STATIC i32 calculate_needed(BpTreeNodeImpl *node, BpTreeItem *item) {
	if (node->is_internal) {
		if (item->item_type != BPTREE_ITEM_TYPE_INTERNAL) return -1;
		return item->key_len + sizeof(BpTreeInternalEntry);
	} else if (item->item_type == BPTREE_ITEM_TYPE_OVERFLOW) {
		return item->key_len + sizeof(BpTreeLeafEntry) +
		       sizeof(u64) * 2;
	} else {
		if (item->item_type != BPTREE_ITEM_TYPE_LEAF) return -1;
		return item->key_len + item->vardata.kv.value_len +
		       sizeof(BpTreeLeafEntry);
	}
}

STATIC void shift_by_offset(BpTreeNodeImpl *node, u16 key_index, i32 shift) {
	i32 i;
	u16 pos, bytes_to_move;
	void *dst, *src;

	if (node->is_internal)
		pos = node->data.internal.entry_offsets[key_index];
	else
		pos = node->data.leaf.entry_offsets[key_index];
	bytes_to_move = node->used_bytes - pos;
	if (node->is_internal) {
		dst = node->data.internal.entries + pos + shift;
		src = node->data.internal.entries + pos;

	} else {
		dst = node->data.leaf.entries + pos + shift;
		src = node->data.leaf.entries + pos;
	}

	memorymove(dst, src, bytes_to_move);
	for (i = node->num_entries; i > key_index; i--) {
		if (node->is_internal) {
			node->data.internal.entry_offsets[i] =
			    node->data.internal.entry_offsets[i - 1] + shift;

		} else {
			node->data.leaf.entry_offsets[i] =
			    node->data.leaf.entry_offsets[i - 1] + shift;
		}
	}
}

STATIC void place_item(BpTreeNodeImpl *node, u16 key_index, BpTreeItem *item) {
	if (node->is_internal) {
		u16 pos = node->data.internal.entry_offsets[key_index];
		BpTreeInternalEntry entry = {0};
		entry.key_len = item->key_len;
		entry.node_id = item->vardata.internal.node_id;
		memcpy((u8 *)node->data.internal.entries + pos, &entry,
		       sizeof(BpTreeInternalEntry));
		memcpy((u8 *)node->data.internal.entries + pos +
			   sizeof(BpTreeInternalEntry),
		       item->key, entry.key_len);

	} else {
		u16 pos = node->data.leaf.entry_offsets[key_index];
		BpTreeLeafEntry entry = {0};
		if (item->item_type == BPTREE_ITEM_TYPE_LEAF)
			entry.value_len = item->vardata.kv.value_len;
		else if (item->item_type == BPTREE_ITEM_TYPE_OVERFLOW) {
			entry.value_len = item->vardata.overflow.value_len;
			entry.overflow = true;
		}
		entry.key_len = item->key_len;
		memcpy((u8 *)node->data.leaf.entries + pos, &entry,
		       sizeof(BpTreeLeafEntry));
		memcpy((u8 *)node->data.leaf.entries + pos +
			   sizeof(BpTreeLeafEntry),
		       item->key, entry.key_len);

		if (item->item_type == BPTREE_ITEM_TYPE_LEAF)
			memcpy((u8 *)node->data.leaf.entries + pos +
				   sizeof(BpTreeLeafEntry) + entry.key_len,
			       item->vardata.kv.value, entry.value_len);
		else if (item->item_type == BPTREE_ITEM_TYPE_OVERFLOW) {
			memcpy((u8 *)node->data.leaf.entries + pos +
				   sizeof(BpTreeLeafEntry) + entry.key_len,
			       &item->vardata.overflow.overflow_start,
			       sizeof(u64));
			memcpy((u8 *)node->data.leaf.entries + pos +
				   sizeof(BpTreeLeafEntry) + entry.key_len +
				   sizeof(u64),
			       &item->vardata.overflow.overflow_end,
			       sizeof(u64));
		}
	}
}

i32 bptree_prim_init_node(BpTreeNode *node, u64 parent_id, bool is_internal) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;

	if (!impl) {
		err = EINVAL;
		return -1;
	}

	impl->is_internal = is_internal;
	impl->parent_id = parent_id;
	impl->used_bytes = impl->num_entries = 0;
	impl->is_copy = true;
	if (is_internal)
		impl->data.internal.entry_offsets[0] = 0;
	else
		impl->data.leaf.entry_offsets[0] = 0;

	return 0;
}

i32 bptree_prim_set_aux(BpTreeNode *node, u64 aux) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;

	if (!impl || !impl->is_copy) {
		err = EINVAL;
		return -1;
	}

	impl->aux = aux;
	return 0;
}

i32 bptree_prim_unset_copy(BpTreeNode *node) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;

	if (!impl || impl->is_copy) {
		err = EINVAL;
		return -1;
	}

	impl->is_copy = false;
	return 0;
}

i32 bptree_prim_copy(BpTreeNode *dst, BpTreeNode *src) {
	BpTreeNodeImpl *dst_impl = (BpTreeNodeImpl *)dst;
	if (!dst_impl->is_copy) {
		err = EINVAL;
		return -1;
	}
	memcpy((u8 *)dst_impl, (u8 *)src, NODE_SIZE);
	dst_impl->is_copy = true;
	return 0;
}

i32 bptree_prim_set_next_leaf(BpTreeNode *node, u64 next_leaf) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;

	if (!impl || impl->is_internal || !impl->is_copy) {
		err = EINVAL;
		return -1;
	}

	impl->data.leaf.next_leaf = next_leaf;
	return 0;
}

i32 bptree_prim_insert_entry(BpTreeNode *node, u16 index, BpTreeItem *item) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	i32 needed;

	if (!node || !item || index > impl->num_entries) {
		err = EINVAL;
		return -1;
	}

	needed = calculate_needed(impl, item);
	if (needed < 0) {
		err = EINVAL;
		return -1;
	}

	if (impl->num_entries > index) {
		shift_by_offset(impl, index, needed);
	} else {
		impl->data.leaf.entry_offsets[impl->num_entries] =
		    impl->used_bytes;
	}

	place_item(impl, index, item);
	impl->used_bytes += needed;
	impl->num_entries++;

	return 0;
}

u64 bptree_prim_parent_id(BpTreeNode *node) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	return impl->parent_id;
}

u16 bptree_prim_num_entries(BpTreeNode *node) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	return impl->num_entries;
}

u16 bptree_prim_used_bytes(BpTreeNode *node) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	return impl->used_bytes;
}

bool bptree_prim_is_copy(BpTreeNode *node) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	return impl->is_copy;
}

bool bptree_prim_is_internal(BpTreeNode *node) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	return impl->is_internal;
}

u64 bptree_prim_aux(BpTreeNode *node) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	return impl->aux;
}

u64 bptree_prim_next_leaf(BpTreeNode *node) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	if (!impl || !impl->is_internal) return 0;
	return impl->data.leaf.next_leaf;
}

u16 bptree_prim_key_len(BpTreeNode *node, u16 index) {
	u16 pos;
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;

	if (impl->is_internal) {
		BpTreeInternalEntry *entry;
		pos = impl->data.internal.entry_offsets[index];
		entry =
		    (BpTreeInternalEntry *)((u8 *)impl->data.internal.entries +
					    pos);
		return entry->key_len;
	} else {
		BpTreeLeafEntry *entry;
		pos = impl->data.leaf.entry_offsets[index];
		entry =
		    (BpTreeLeafEntry *)((u8 *)impl->data.leaf.entries + pos);
		return entry->key_len;
	}
}

u32 bptree_prim_value_len(BpTreeNode *node, u16 index) {
	BpTreeLeafEntry *entry;
	u16 pos;
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	if (impl->is_internal) return 0;
	pos = impl->data.leaf.entry_offsets[index];
	entry = (BpTreeLeafEntry *)((u8 *)impl->data.leaf.entries + pos);
	return entry->value_len;
}

const void *bptree_prim_key(BpTreeNode *node, u16 index) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	u16 pos;

	if (impl->is_internal) {
		pos = impl->data.internal.entry_offsets[index];
		return (const void *)((u8 *)impl->data.internal.entries + pos +
				      sizeof(BpTreeInternalEntry));
	} else {
		pos = impl->data.leaf.entry_offsets[index];
		return (const void *)((u8 *)impl->data.leaf.entries + pos +
				      sizeof(BpTreeLeafEntry));
	}
}

const void *bptree_prim_value(BpTreeNode *node, u16 index) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	u16 pos = impl->data.leaf.entry_offsets[index];
	BpTreeLeafEntry *entry =
	    (BpTreeLeafEntry *)((u8 *)impl->data.leaf.entries + pos);

	if (impl->is_internal || entry->overflow) {
		err = EINVAL;
		return NULL;
	}

	return (const void *)((u8 *)impl->data.leaf.entries + pos +
			      sizeof(BpTreeLeafEntry) + entry->key_len);
}

int bptree_prim_is_overflow(BpTreeNode *node, u16 index) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	u16 pos;
	BpTreeLeafEntry *entry;

	if (index >= impl->num_entries) {
		err = EINVAL;
		return -1;
	}

	pos = impl->data.leaf.entry_offsets[index];
	entry = (BpTreeLeafEntry *)((u8 *)impl->data.leaf.entries + pos);

	if (entry->overflow) return 1;
	return 0;
}

u64 bptree_prim_overflow_start(BpTreeNode *node, u16 index) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	u16 pos;
	BpTreeLeafEntry *entry;

	if (index >= impl->num_entries) {
		err = EINVAL;
		return -1;
	}

	pos = impl->data.leaf.entry_offsets[index];
	entry = (BpTreeLeafEntry *)((u8 *)impl->data.leaf.entries + pos);

	if (entry->overflow) {
		u64 start;
		memcpy(&start,
		       (u8 *)impl->data.leaf.entries + pos +
			   sizeof(BpTreeLeafEntry) + entry->key_len,
		       sizeof(u64));
		return start;
	}
	err = EINVAL;
	return 0;
}

u64 bptree_prim_overflow_end(BpTreeNode *node, u16 index) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	u16 pos;
	BpTreeLeafEntry *entry;

	if (index >= impl->num_entries) {
		err = EINVAL;
		return -1;
	}

	pos = impl->data.leaf.entry_offsets[index];
	entry = (BpTreeLeafEntry *)((u8 *)impl->data.leaf.entries + pos);

	if (entry->overflow) {
		u64 end;
		memcpy(&end,
		       (u8 *)impl->data.leaf.entries + pos +
			   sizeof(BpTreeLeafEntry) + entry->key_len +
			   sizeof(u64),
		       sizeof(u64));
		return end;
	}
	err = EINVAL;
	return 0;
}

