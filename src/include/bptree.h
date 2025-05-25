#ifndef _BPTREE_H__
#define _BPTREE_H__

#include <types.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE (4096 * 4)
#endif /* PAGE_SIZE */

#define MAX_ENTRIES (PAGE_SIZE / 32)
#define KEY_ARRAY_SIZE ((11 * PAGE_SIZE - 640) / 16)
#define ENTRY_ARRAY_SIZE ((29 * PAGE_SIZE - 1280) / 32)

typedef struct {
	byte opaque[24];
} BpTree;

typedef struct {
	uint16_t num_entries;
	uint16_t entry_offsets[MAX_ENTRIES];
	uint64_t children[MAX_ENTRIES + 1];
	uint8_t key_data[KEY_ARRAY_SIZE];
} BpTreeInternalNode;

typedef struct {
	uint16_t num_entries;
	uint16_t used_bytes;
	uint16_t entry_offsets[MAX_ENTRIES];
	uint8_t entries[ENTRY_ARRAY_SIZE];
	uint8_t is_overflow[MAX_ENTRIES];
	uint64_t next_leaf : 48;
} BpTreeLeafNode;

typedef struct {
	uint64_t page_id : 48;
	uint64_t parent_id : 48;
	uint64_t free_list_next : 48;
	uint8_t is_leaf : 1;
	union {
		BpTreeInternalNode internal;
		BpTreeLeafNode leaf;
	} data;
} BpTreeNode;

typedef struct {
	uint64_t parent_page_id : 48;
	uint64_t self_page_id : 48;
	uint16_t key_index;
	bool found;
} BpTreeNodeSearchResult;

typedef struct {
	byte opaque[56];
} BpTxn;

typedef enum {
	FileBacked = 0,
	InMemory = 1,
} BpTreeType;

typedef void (*BpTreeSearch)(BpTxn *txn, const void *key, uint16_t key_len,
			     const BpTreeNode *node,
			     BpTreeNodeSearchResult *retval);

int bptree_init(BpTree *tree, void *base, uint64_t capacity, BpTreeType btype);
int bptree_put(BpTxn *txn, const void *key, uint16_t key_len, const void *value,
	       uint64_t value_len, const BpTreeSearch search);
void *bptree_remove(BpTxn *txn, const void *key, uint16_t key_len,
		    const void *value, uint64_t value_len,
		    const BpTreeSearch search);

void bptxn_start(BpTxn *txn, BpTree *tree);
int bptxn_commit(BpTxn *txn);
int bptxn_abort(BpTxn *txn);

#endif /* _BPTREE_H__ */
