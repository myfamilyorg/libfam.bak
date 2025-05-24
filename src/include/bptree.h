#ifndef _BPTREE_H__
#define _BPTREE_H__

#include <types.h>

#define PAGE_SIZE (4096 * 4)
#define MAX_KEYS 100
#define MAX_ENTRIES 100
#define ENTRY_ARRAY_SIZE 2048
#define KEY_ARRAY_SIZE 2048

#define METADATA1(tree) (Metadata *)((char *)tree->base)
#define METADATA2(tree) (Metadata *)((char *)(((size_t)tree->base) + PAGE_SIZE))
#define FREE_LIST(tree) \
	(FreeList *)((char *)(((size_t)tree->base) + 2 * PAGE_SIZE))
#define NODE(tree, index) \
	((BpTreeNode *)(((size_t)tree->base) + (index * PAGE_SIZE)))
#define MIN_PAGE 3
#define COPY_NODE(tree, dst, src) \
	memcpy(NODE(tree, dst), NODE(tree, src), PAGE_SIZE)
#define COPY_KEY_LEAF(leaf, key, key_len, key_index)                    \
	do {                                                            \
		memcpy(leaf->entries + leaf->entry_offsets[key_index],  \
		       &key_len, sizeof(uint16_t));                     \
		memcpy(leaf->entries + leaf->entry_offsets[key_index] + \
			   sizeof(uint16_t),                            \
		       key, key_len);                                   \
	} while (false);
#define COPY_KEY_VALUE_LEAF(leaf, key, key_len, value, value_len, key_index) \
	do {                                                                 \
		COPY_KEY_LEAF(leaf, key, key_len, key_index);                \
		memcpy(leaf->entries + leaf->entry_offsets[key_index] +      \
			   sizeof(uint16_t) + key_len,                       \
		       &value_len, sizeof(uint64_t));                        \
		memcpy(leaf->entries + leaf->entry_offsets[key_index] +      \
			   sizeof(uint64_t) + sizeof(uint16_t) + key_len,    \
		       value, value_len);                                    \
	} while (false);

#define NEEDED_KEY(key_len) (key_len + sizeof(uint16_t))
#define NEEDED_KEY_VALUE(key_len, value_len) \
	(NEEDED_KEY(key_len) + value_len + sizeof(uint64_t))

typedef struct {
	void *base;
	int fd;
	uint64_t capacity;
} BpTree;

// Internal (non-leaf) node, stores variable-length keys and child page IDs
typedef struct BpTreeInternalNode {
	uint16_t num_entries;		      // Number of keys (separator keys)
	uint16_t entry_offsets[MAX_ENTRIES];  // Offsets in entries array
	uint64_t children[MAX_ENTRIES + 1];   // Child page IDs
	uint8_t key_data[KEY_ARRAY_SIZE];     // key data
} BpTreeInternalNode;

// Leaf node, stores variable-length key-value pairs
typedef struct BpTreeLeafNode {
	uint16_t num_entries;		      // Number of key-value pairs
	uint16_t used_bytes;		      // Total bytes used by entries
	uint16_t entry_offsets[MAX_ENTRIES];  // Offsets in entries array
	uint8_t entries[ENTRY_ARRAY_SIZE];    // Serialized key-value pairs
	uint8_t is_overflow[MAX_ENTRIES];     // 1 if entry is overflow page ID
	uint64_t next_leaf;  // Next leaf page ID (for range queries)
} BpTreeLeafNode;

typedef struct BpTreeNode {
	uint64_t page_id;  // Unique page ID (e.g., 1007), for mmap offset
	uint8_t is_leaf;   // 1 for leaf, 0 for internal
	uint64_t free_list_next;  // next free node
	union {
		BpTreeInternalNode internal;
		BpTreeLeafNode leaf;
	} data;
} BpTreeNode;

typedef struct BpTreeNodePair {
	uint64_t parent_page_id;  // Page ID of parent node (e.g., 4)
	uint64_t self_page_id;	  // Page ID of current node (e.g., 7)
	uint16_t key_index;	  // Index of key in parent’s children array
} BpTreeNodePair;

typedef struct {
	BpTree *tree;
	uint64_t txn_id;
	uint64_t new_root;
	uint64_t *pending_free;
	size_t pending_free_count;
	uint64_t *modified_pages;
	size_t modified_pages_count;
} Txn;

typedef int (*BpTreeSearch)(Txn *txn, const void *key, uint16_t key_len,
			    const BpTreeNode *node, BpTreeNodePair *retval);

int bptree_init(BpTree *tree);
int bptree_put(Txn *txn, const void *key, uint16_t key_len, const void *value,
	       uint64_t value_len, const BpTreeSearch search);
void *bptree_remove(Txn *txn, const void *key, uint16_t key_len,
		    const void *value, uint64_t value_len,
		    const BpTreeSearch search);

#endif /* _BPTREE_H__ */
