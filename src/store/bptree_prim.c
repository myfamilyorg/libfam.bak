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
} BpTreeEntry;

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

STATIC void shift_leaf_by_offset(BpTreeNodeImpl *node, u16 key_index,
				 i32 shift) {
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

STATIC void place_leaf_entry(BpTreeNodeImpl *node, u16 key_index,
			     const void *key, u16 key_len, const void *value,
			     u32 value_len) {
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

STATIC u64 space_needed(BpTreeItem *item, bool is_internal) {
	if (is_internal)
		return item->key_len + sizeof(BpTreeInternalEntry);
	else
		return item->key_len + item->value_len + sizeof(BpTreeEntry);
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

i32 bptree_prim_move_leaf_node_entries(BpTreeNode *dst, u16 dst_start_index,
				       BpTreeNode *src, u16 src_start_index,
				       u16 num_entries) {
	BpTreeNodeImpl *dst_impl = (BpTreeNodeImpl *)dst;
	BpTreeNodeImpl *src_impl = (BpTreeNodeImpl *)src;
	u32 dst_start_pos, src_start_pos, bytes_to_copy, i;

	/* Return early for no entries */
	if (num_entries == 0) {
		return 0;
	}

	/* Validate inputs */
	if (dst_impl->is_internal || src_impl->is_internal ||
	    !dst_impl->is_copy || !src_impl->is_copy ||
	    dst_start_index > dst_impl->num_entries ||
	    src_start_index + num_entries < src_start_index ||
	    src_start_index + num_entries > src_impl->num_entries) {
		return -1;
	}

	/* Calculate byte positions and size to copy */
	dst_start_pos = dst_impl->data.leaf.entry_offsets[dst_start_index];
	src_start_pos = src_impl->data.leaf.entry_offsets[src_start_index];
	if (src_start_index + num_entries < src_impl->num_entries)
		bytes_to_copy =
		    src_impl->data.leaf
			.entry_offsets[src_start_index + num_entries] -
		    src_start_pos;
	else
		bytes_to_copy = src_impl->used_bytes - src_start_pos;

	/* Check destination capacity */
	if (dst_impl->used_bytes + bytes_to_copy > LEAF_ARRAY_SIZE) {
		return -1;
	}

	/* Shift destination entries to make space */
	if (dst_start_index < dst_impl->num_entries)
		shift_leaf_by_offset(dst_impl, dst_start_index, bytes_to_copy);

	/* Copy entries from source to destination */
	memorymove((u8 *)dst_impl->data.leaf.entries + dst_start_pos,
		   (u8 *)src_impl->data.leaf.entries + src_start_pos,
		   bytes_to_copy);

	/* Update metadata */
	dst_impl->num_entries += num_entries;
	src_impl->num_entries -= num_entries;
	dst_impl->used_bytes += bytes_to_copy;
	src_impl->used_bytes -= bytes_to_copy;

	/* Adjust destination offsets for moved entries */
	for (i = 0; i < num_entries; i++) {
		dst_impl->data.leaf.entry_offsets[dst_start_index + i] =
		    src_impl->data.leaf.entry_offsets[src_start_index + i] -
		    src_start_pos + dst_start_pos;
	}

	/* Compact source node by shifting remaining entries left */
	if (src_start_index + num_entries <
	    src_impl->num_entries + num_entries) {
		u32 bytes_to_shift =
		    src_impl->used_bytes; /* After metadata update */
		if (src_impl->num_entries > 0) {
			/* Move remaining data to start of entries */
			memorymove((u8 *)src_impl->data.leaf.entries,
				   (u8 *)src_impl->data.leaf.entries +
				       src_start_pos + bytes_to_copy,
				   bytes_to_shift);
			/* Update offsets for remaining entries */
			for (i = 0; i < src_impl->num_entries; i++) {
				src_impl->data.leaf.entry_offsets[i] =
				    src_impl->data.leaf
					.entry_offsets[src_start_index +
						       num_entries + i] -
				    (src_start_pos + bytes_to_copy);
			}
		}
	}

	return 0;
}

i32 bptree_prim_set_leaf_entry(BpTreeNode *node, u16 index, BpTreeItem *item) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	u32 old_pos, old_size, new_size;
	i32 size_diff;
	BpTreeEntry *entry;

	if (!node || !item || !impl->is_copy || index >= impl->num_entries ||
	    index >= MAX_LEAF_ENTRIES || item->is_overflow ||
	    item->key_len == 0 || item->value_len == 0 ||
	    !item->vardata.kv.key || !item->vardata.kv.value) {
		err = EINVAL;
		return -1;
	}

	/* Get old entry details */
	old_pos = impl->data.leaf.entry_offsets[index];
	if (old_pos >= LEAF_ARRAY_SIZE) {
		err = EINVAL;
		return -1;
	}
	entry = (BpTreeEntry *)((u8 *)impl->data.leaf.entries + old_pos);
	if (entry->key_len > LEAF_ARRAY_SIZE ||
	    entry->value_len > LEAF_ARRAY_SIZE) {
		err = EINVAL;
		return -1;
	}
	old_size = entry->key_len + entry->value_len + sizeof(BpTreeEntry);

	/* Calculate new entry size */
	new_size = item->key_len + item->value_len + sizeof(BpTreeEntry);
	if (new_size > LEAF_ARRAY_SIZE ||
	    old_pos + new_size > LEAF_ARRAY_SIZE) {
		err = EOVERFLOW;
		return -1;
	}

	/* Calculate size difference */
	size_diff = (i32)new_size - (i32)old_size;
	if (impl->used_bytes + size_diff > (i32)LEAF_ARRAY_SIZE ||
	    (size_diff < 0 && impl->used_bytes < (u32)-size_diff)) {
		err = EOVERFLOW;
		return -1;
	}

	/* Shift subsequent entries if size changes and not last entry */
	if (size_diff != 0 && index + 1 < impl->num_entries) {
		u16 i;
		u16 pos = impl->data.leaf.entry_offsets[index + 1];
		void *dst = impl->data.leaf.entries + pos + size_diff;
		void *src = impl->data.leaf.entries + pos;
		u16 bytes_to_move = impl->used_bytes - pos;

		/*shift_leaf_by_offset(impl, index + 1, size_diff);*/
		memorymove(dst, src, bytes_to_move);
		for (i = impl->num_entries; i > index; i--) {
			impl->data.leaf.entry_offsets[i] =
			    impl->data.leaf.entry_offsets[i] + size_diff;
		}
	}

	/* Write new entry */
	entry = (BpTreeEntry *)((u8 *)impl->data.leaf.entries + old_pos);
	entry->key_len = item->key_len;
	entry->value_len = item->value_len;
	entry->overflow = item->is_overflow;
	memorymove((u8 *)entry + sizeof(BpTreeEntry), item->vardata.kv.key,
		   item->key_len);
	memorymove((u8 *)entry + sizeof(BpTreeEntry) + item->key_len,
		   item->vardata.kv.value, item->value_len);

	/* Update used_bytes */
	impl->used_bytes += size_diff;

	return 0;
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

	if (!impl) {
		err = EINVAL;
		return -1;
	}

	impl->aux = aux;
	return 0;
}

i32 bptree_prim_unset_copy(BpTreeNode *node) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;

	if (!impl) {
		err = EINVAL;
		return -1;
	}

	impl->is_copy = false;
	return 0;
}

i32 bptree_prim_insert_leaf_entry(BpTreeNode *node, u16 index,
				  BpTreeItem *item) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	u64 needed;
	if (!impl || !item || index > impl->num_entries ||
	    impl->num_entries >= MAX_LEAF_ENTRIES || !impl->is_copy) {
		err = EINVAL;
		return -1;
	}

	needed = space_needed(item, false);

	if (impl->num_entries > index) {
		shift_leaf_by_offset(impl, index, needed);
	} else {
		impl->data.leaf.entry_offsets[impl->num_entries] =
		    impl->used_bytes;
	}
	place_leaf_entry(impl, index, item->vardata.kv.key, item->key_len,
			 item->vardata.kv.value, item->value_len);
	impl->used_bytes += needed;
	impl->num_entries++;

	return 0;
}

u64 bptree_prim_get_parent_id(BpTreeNode *node) {
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
		BpTreeEntry *entry;
		pos = impl->data.leaf.entry_offsets[index];
		entry = (BpTreeEntry *)((u8 *)impl->data.leaf.entries + pos);
		return entry->key_len;
	}
}

u32 bptree_prim_value_len(BpTreeNode *node, u16 index) {
	BpTreeEntry *entry;
	u16 pos;
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	if (impl->is_internal) return 0;
	pos = impl->data.leaf.entry_offsets[index];
	entry = (BpTreeEntry *)((u8 *)impl->data.leaf.entries + pos);
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
				      sizeof(BpTreeEntry));
	}
}

const void *bptree_prim_value(BpTreeNode *node, u16 index) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	u16 pos = impl->data.leaf.entry_offsets[index];
	BpTreeEntry *entry =
	    (BpTreeEntry *)((u8 *)impl->data.leaf.entries + pos);

	if (impl->is_internal) {
		err = EINVAL;
		return NULL;
	}

	return (const void *)((u8 *)impl->data.leaf.entries + pos +
			      sizeof(BpTreeEntry) + entry->key_len);
}

bool bptree_prim_is_copy(BpTreeNode *node) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	return impl->is_copy;
}

bool bptree_prim_is_internal(BpTreeNode *node) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	return impl->is_internal;
}

u64 bptree_prim_get_aux(BpTreeNode *node) {
	BpTreeNodeImpl *impl = (BpTreeNodeImpl *)node;
	return impl->aux;
}
