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

#ifndef _BPTREE_H
#define _BPTREE_H

#include <types.h>

#ifndef NODE_SIZE
#ifndef PAGE_SIZE
#define PAGE_SIZE (4 * 4096)
#endif /* PAGE_SIZE */
#define NODE_SIZE PAGE_SIZE
#endif /* NODE_SIZE */

#define MAX_LEAF_ENTRIES (NODE_SIZE / 32)
#define MAX_INTERNAL_ENTRIES (NODE_SIZE / 16)
#define INTERNAL_ARRAY_SIZE ((28 * NODE_SIZE - 1024) / 32)
#define LEAF_ARRAY_SIZE ((30 * NODE_SIZE - 1280) / 32)

/* Main opaque pointers for tree and txn */
typedef struct BpTree BpTree;
typedef struct BpTxn BpTxn;

/* The result returned in a search, used by put/remove */
typedef struct {
	uint64_t page_id;
	uint16_t key_index;
	bool found;
} BpTreeSearchResult;

/* BpTree entry */
typedef struct {
	uint32_t value_len;
	uint16_t key_len;
	uint8_t flags;
} BpTreeEntry;

/* Data specific to an internal page */
typedef struct {
	uint16_t entry_offsets[MAX_INTERNAL_ENTRIES];
	uint8_t key_entries[INTERNAL_ARRAY_SIZE];
} BpTreeInternalNode;

/* Data specific to a leaf page */
typedef struct {
	uint64_t next_leaf;
	uint16_t entry_offsets[MAX_LEAF_ENTRIES];
	uint8_t entries[LEAF_ARRAY_SIZE];
} BpTreeLeafNode;

/* BpTree Node */
typedef struct {
	uint64_t page_id;
	uint64_t parent_id;
	uint64_t free_list_next;
	uint16_t num_entries;
	uint16_t used_bytes;
	uint8_t is_leaf;
	union {
		BpTreeInternalNode internal;
		BpTreeLeafNode leaf;
	} data;
} BpTreeNode;

/* Opening/Closing */
BpTree *bptree_open(const char *path);
int bptree_close(BpTree *);

/* Txn management */
BpTxn *bptxn_start(BpTree *tree);
int bptxn_commit(BpTxn *txn);
int bptxn_abort(BpTxn *txn);

/* User defined search */
typedef void (*BpTreeSearch)(BpTxn *txn, const void *key, uint16_t key_len,
			     const BpTreeNode *page,
			     BpTreeSearchResult *retval);

/* Modification functions */
int bptree_put(BpTxn *txn, const void *key, uint16_t key_len, const void *value,
	       uint32_t value_len, const BpTreeSearch search);
BpTreeEntry *bptree_remove(BpTxn *txn, const void *key, uint16_t key_len,
			   const void *value, uint64_t value_len,
			   const BpTreeSearch search);

/* Helpers needed for search */
BpTreeNode *bptxn_get_page(BpTxn *txn, uint64_t page_id);
BpTreeNode *bptree_root(BpTxn *tree);

#endif /* _BPTREE_H */
