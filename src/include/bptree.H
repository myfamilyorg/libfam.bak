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

#include <types.H>
#include <storage.H>

#ifndef NODE_SIZE
#ifndef PAGE_SIZE
#define PAGE_SIZE (4 * 4096)
#endif /* PAGE_SIZE */
#define NODE_SIZE ((u64)PAGE_SIZE)
#endif /* NODE_SIZE */     

#define MAX_LEAF_ENTRIES ((u64)(NODE_SIZE / 32))
#define MAX_INTERNAL_ENTRIES (NODE_SIZE / 16)
#define INTERNAL_ARRAY_SIZE ((28 * NODE_SIZE - 1024) / 32)
#define LEAF_ARRAY_SIZE ((u64)((30 * NODE_SIZE - 1280) / 32))

#define MAX_LEVELS 32

typedef struct BpTree BpTree;
typedef struct BpTxn BpTxn;

/* The result returned in a search, used by put/remove */
typedef struct {
        u64 node_id;
        u16 parent_index[MAX_LEVELS];
        u8 levels;
        u16 key_index;
        bool found;
} BpTreeSearchResult;

/* BpTree entry */
typedef struct {
        u32 value_len;
        u16 key_len;
        u8 flags;
} BpTreeEntry;

/* BpTree Internal entry */
typedef struct {
        u16 key_len;
        u64 node_id;
} BpTreeInternalEntry;

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
        u64 next_bptxn_id;
        /* Reserved potentiallly for suffix tree / other data */
} MetaData;

/* BpTree Node */
typedef struct {
	MetaData *meta;
        u64 parent_id;
	u64 next_ptr;
        u16 num_entries;
        u16 used_bytes;
        bool is_internal;
        bool is_copy;
        union {
                BpTreeInternalNode internal;
                BpTreeLeafNode leaf;
        } data;
} BpTreeNode;

/* User defined search */
typedef void (*BpTreeSearch)(BpTxn *txn, const void *key, u16 key_len,
                             const BpTreeNode *node,
                             BpTreeSearchResult *retval);

/* Opening */
BpTree *bptree_open(Env *env);

/* Txn management */
BpTxn *bptxn_start(BpTree *tree);
i64 bptxn_commit(BpTxn *txn, int wakeupfd);
i32 bptxn_abort(BpTxn *txn);

/* Modification functions */
i32 bptree_put(BpTxn *txn, const void *key, u16 key_len, const void *value,
               u32 value_len, const BpTreeSearch search);
BpTreeEntry *bptree_remove(BpTxn *txn, const void *key, u16 key_len,
                           const void *value, u64 value_len,
                           const BpTreeSearch search);

/* Helpers needed for search */
BpTreeNode *bptxn_get_node(BpTxn *txn, u64 node_id);
BpTreeNode *bptree_root(BpTxn *txn);
u64 bptree_node_id(BpTxn *txn, const BpTreeNode *node);

#endif /* _BPTREE_H */
