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

#ifndef _BPTREE_H__
#define _BPTREE_H__

#include <types.h>

#define MAX_LEAF_ENTRIES (PAGE_SIZE / 32)
#define MAX_INTERNAL_ENTRIES (PAGE_SIZE / 16)
#define INTERNAL_ARRAY_SIZE ((28 * PAGE_SIZE - 1024) / 32)
#define LEAF_ARRAY_SIZE ((30 * PAGE_SIZE - 1280) / 32)

typedef struct {
	byte opaque[24];
} BpTree;

typedef struct {
	byte opaque[40];
} BpTxn;

typedef struct {
	uint64_t page_id;
	uint16_t key_index;
	bool found;
} BpTreeSearchResult;

typedef struct {
	uint32_t value_len;
	uint16_t key_len;
	uint8_t flags;
} BpTreeEntry;

typedef struct {
	uint16_t entry_offsets[MAX_INTERNAL_ENTRIES];
	uint8_t key_entries[INTERNAL_ARRAY_SIZE];
} BpTreeInternalPage;

typedef struct {
	uint64_t next_leaf;
	uint16_t entry_offsets[MAX_LEAF_ENTRIES];
	uint8_t entries[LEAF_ARRAY_SIZE];
} BpTreeLeafPage;

typedef struct {
	uint64_t page_id;
	uint64_t parent_id;
	uint64_t free_list_next;
	uint16_t num_entries;
	uint16_t used_bytes;
	uint8_t is_leaf;
	union {
		BpTreeInternalPage internal;
		BpTreeLeafPage leaf;
	} data;
} BpTreePage;

typedef void (*BpTreeSearch)(BpTxn *txn, const void *key, uint16_t key_len,
			     const BpTreePage *page,
			     BpTreeSearchResult *retval);

int bptree_open(BpTree *, const char *path);
int bptree_close(BpTree *);
int bptree_put(BpTxn *txn, const void *key, uint16_t key_len, const void *value,
	       uint32_t value_len, const BpTreeSearch search);
void *bptree_remove(BpTxn *txn, const void *key, uint16_t key_len,
		    const void *value, uint64_t value_len,
		    const BpTreeSearch search);

void bptxn_start(BpTxn *txn, BpTree *tree);
int bptxn_commit(BpTxn *txn);
int bptxn_abort(BpTxn *txn);

BpTreePage *bptxn_get_page(BpTxn *txn, uint64_t page_id);
BpTreePage *bptree_root(BpTxn *tree);

#endif /* _BPTREE_H__ */
