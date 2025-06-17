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
#include <bptree.H>
#include <error.H>
#include <rng.H>
#include <test.H>

void __attribute__((unused)) print_node(BpTxn *txn, const BpTreeNode *node,
					u8 *prefix) {
	u64 node_id = bptree_node_id(txn, node);
	if (node->is_internal) {
		u16 i;
		printf(
		    "%s\tInternal "
		    "num_entries=%i,used_bytes=%i,parent=%i,is_copy=%i,node_id="
		    "%i\n",
		    prefix, node->num_entries, node->used_bytes,
		    node->parent_id, node->is_copy, node_id);
		for (i = 0; i < node->num_entries; i++) {
			char tmp[NODE_SIZE];
			u16 offset = node->data.internal.entry_offsets[i];
			BpTreeInternalEntry *entry =
			    (BpTreeInternalEntry
				 *)((u8 *)node->data.internal.entries + offset);

			memcpy(tmp,
			       node->data.internal.entries + offset +
				   sizeof(BpTreeInternalEntry),
			       entry->key_len);
			tmp[entry->key_len] = 0;
			printf("%s\t\tkey[%u(len=%u,off=%u)]='%s',node_id=%u\n",
			       prefix, i, entry->key_len, offset, tmp,
			       entry->node_id);
		}
		for (i = 0; i < node->num_entries; i++) {
			u16 offset = node->data.internal.entry_offsets[i];
			BpTreeInternalEntry *entry =
			    (BpTreeInternalEntry
				 *)((u8 *)node->data.internal.entries + offset);

			BpTreeNode *next = bptxn_get_node(txn, entry->node_id);
			printf("\n");
			print_node(txn, next, "\t");
		}
	} else {
		int i;
		printf(
		    "%s\tLeaf "
		    "num_entries=%i,used_bytes=%i,parent=%i,is_copy=%i,node_id="
		    "%i\n",
		    prefix, node->num_entries, node->used_bytes,
		    node->parent_id, node->is_copy, node_id);
		for (i = 0; i < node->num_entries; i++) {
			char tmp[NODE_SIZE];
			u16 offset = node->data.leaf.entry_offsets[i];
			BpTreeEntry *entry =
			    (BpTreeEntry *)((u8 *)node->data.leaf.entries +
					    offset);

			memcpy(tmp,
			       node->data.leaf.entries + offset +
				   sizeof(BpTreeEntry),
			       entry->key_len);
			tmp[entry->key_len] = 0;
			printf(
			    "%s\t\tkey[%u(len=%u,off=%u)]='%s',value_len=%"
			    "u\n",
			    prefix, i, entry->key_len, offset, tmp,
			    entry->value_len);
		}
	}
}

void __attribute__((unused)) print_tree(BpTxn *txn) {
	BpTreeNode *root = bptree_root(txn);
	printf(
	    "------------------------------------------------------------------"
	    "--------------------------\n");

	printf("Printing tree\n");
	printf(
	    "------------------------------------------------------------------"
	    "--------------------------\n");
	print_node(txn, root, "");

	printf(
	    "------------------------------------------------------------------"
	    "--------------------------\n");
}

void test_bptree_search(BpTxn *txn, const void *key, u16 key_len,
			const BpTreeNode *node, BpTreeSearchResult *retval) {
	i32 i;
	retval->found = false;

	while (node->is_internal) {
		u16 offset = node->data.internal.entry_offsets[1];
		BpTreeInternalEntry *entry =
		    (BpTreeInternalEntry *)((u8 *)node->data.internal.entries +
					    offset);
		const u8 *cmp_key __attribute__((unused)) =
		    (const u8 *)((u8 *)node->data.internal.entries + offset +
				 sizeof(BpTreeInternalEntry));
		u16 len = key_len > entry->key_len ? entry->key_len : key_len;
		i32 v = strcmpn(key, cmp_key, len);

		if (v >= 0) {
			node = bptxn_get_node(txn, entry->node_id);
		} else {
			entry = (BpTreeInternalEntry *)((u8 *)node->data
							    .internal.entries);
			node = bptxn_get_node(txn, entry->node_id);
		}
		break;
	}

	for (i = 0; i < node->num_entries; i++) {
		i32 v;
		u16 offset = node->data.leaf.entry_offsets[i];
		BpTreeEntry *entry =
		    (BpTreeEntry *)((u8 *)node->data.leaf.entries + offset);
		const u8 *cmp_key =
		    (const u8 *)((u8 *)entry + sizeof(BpTreeEntry));
		u16 len = key_len > entry->key_len ? entry->key_len : key_len;

		v = strcmpn(key, cmp_key, len);

		{
			u8 tmp[1024] = {0};
			memcpy(tmp, cmp_key, entry->key_len);
			tmp[entry->key_len] = 0;
		}

		if (v == 0 && key_len == entry->key_len) {
			retval->found = true;
			break;
		}
		if (v < 0) {
			break;
		}
	}

	retval->node_id = bptree_node_id(txn, node);
	retval->key_index = i;
}

Test(simple_split) {
	const u8 *path = "/tmp/store1.dat";
	i32 fd, i, v;
	BpTree *tree;
	BpTxn *txn;
	u8 key1[16384], key2[16384], key3[16384], key4[16384], key5[16384];
	u8 value1[16384], value2[16384], value3[16384], value4[16384],
	    value5[16384];

	unlink(path);
	fd = file(path);

	fresize(fd, PAGE_SIZE * 16);
	close(fd);
	tree = bptree_open(path);
	ASSERT(tree, "tree");
	txn = bptxn_start(tree);
	ASSERT(txn, "txn");

	for (i = 0; i < 16384; i++) {
		key1[i] = value1[i] = 'a';
		key2[i] = value2[i] = 'b';
		key3[i] = value3[i] = 'c';
		key4[i] = value4[i] = 'd';
		key5[i] = value5[i] = 'e';
	}

	print_tree(txn);
	v = bptree_put(txn, key1, 16, value1, 7600, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	v = bptree_put(txn, key2, 16, value2, 512, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	v = bptree_put(txn, key3, 16, value3, 100, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	v = bptree_put(txn, key4, 16, value4, 7600, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	v = bptree_put(txn, key5, 16, value5, 12, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	key1[1] = 'b';
	v = bptree_put(txn, key1, 16, value1, 12, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	bptxn_abort(txn);
	bptree_close(tree);
	release(tree);
	release(txn);
	unlink(path);
}

/*
Test(store1) {
	const u8 *path = "/tmp/store1.dat";
	i32 fd, i, x;
	BpTree *tree;
	BpTxn *txn;
	Rng rng;
	u8 keys[11][20];
	u8 values[11][20];
	u8 rand_key_lens[11];
	u8 rand_value_lens[11];

	u8 key[32] = {3};
	u8 iv[16] = {2};

	rng_test_seed(&rng, key, iv);

	for (x = 0; x < 1000; x++) {
		unlink(path);
		fd = file(path);

		fresize(fd, PAGE_SIZE * 16);
		close(fd);
		tree = bptree_open(path);
		ASSERT(tree, "tree");
		txn = bptxn_start(tree);

		for (i = 1; i < 11; i++) {
			u8 keybuf[20] = {0};
			u8 valuebuf[20] = {0};
			u64 klen = 0, vlen = 0;

			rng_gen(&rng, keybuf, 20);
			rng_gen(&rng, valuebuf, 20);
			klen = b64_encode(keybuf, 12, keys[i], 20);
			vlen = b64_encode(valuebuf, 12, values[i], 20);
			keys[i][klen] = 0;
			values[i][vlen] = 0;
			rng_gen(&rng, &klen, sizeof(u64));
			rng_gen(&rng, &vlen, sizeof(u64));
			rand_key_lens[i] = (klen % 10) + 3;
			rand_value_lens[i] = (vlen % 10) + 3;
		}

		for (i = 1; i < 11; i++) {
			u8 key_buf[20], value_buf[20];
			i32 v;

			memcpy(key_buf, keys[i], 20);
			memcpy(value_buf, values[i], 20);
			key_buf[rand_key_lens[i]] = 0;
			value_buf[rand_value_lens[i]] = 0;

			v = bptree_put(txn, keys[i], rand_key_lens[i],
				       values[i], rand_value_lens[i],
				       test_bptree_search);
			ASSERT(!v, "insert");
		}

		for (i = 1; i < 11; i++) {
			i32 v = bptree_put(txn, keys[i], rand_key_lens[i],
					   values[i], rand_value_lens[i],
					   test_bptree_search);
			ASSERT(v, "insertcheck");
		}

		bptree_close(tree);
		release(tree);
		release(txn);
		unlink(path);
	}
}

#define BPTREE_SPLIT_ITT 130

Test(bptree_split) {
	const u8 *path = "/tmp/store1.dat";
	i32 fd, i;
	BpTree *tree;
	BpTxn *txn;
	Rng rng;
	u8 keys[BPTREE_SPLIT_ITT][20];
	u8 values[BPTREE_SPLIT_ITT][20];
	u8 rand_key_lens[BPTREE_SPLIT_ITT];
	u8 rand_value_lens[BPTREE_SPLIT_ITT];

	u8 key[32] = {2};
	u8 iv[16] = {1};

	rng_test_seed(&rng, key, iv);
	unlink(path);
	fd = file(path);

	fresize(fd, PAGE_SIZE * 1600);
	close(fd);
	tree = bptree_open(path);
	ASSERT(tree, "tree");
	txn = bptxn_start(tree);

	for (i = 1; i < BPTREE_SPLIT_ITT; i++) {
		u8 keybuf[20] = {0};
		u8 valuebuf[20] = {0};
		u64 klen = 0, vlen = 0;

		rng_gen(&rng, keybuf, 20);
		rng_gen(&rng, valuebuf, 20);
		klen = b64_encode(keybuf, 12, keys[i], 20);
		vlen = b64_encode(valuebuf, 12, values[i], 20);
		keys[i][klen] = 0;
		values[i][vlen] = 0;
		rng_gen(&rng, &klen, sizeof(u64));
		rng_gen(&rng, &vlen, sizeof(u64));
		rand_key_lens[i] = (klen % 10) + 3;
		rand_value_lens[i] = (vlen % 10) + 3;
	}

	for (i = 1; i < BPTREE_SPLIT_ITT; i++) {
		u8 key_buf[20], value_buf[20];
		i32 v;

		memcpy(key_buf, keys[i], 20);
		memcpy(value_buf, values[i], 20);
		key_buf[rand_key_lens[i]] = 0;
		value_buf[rand_value_lens[i]] = 0;

		v = bptree_put(txn, keys[i], rand_key_lens[i], values[i],
			       rand_value_lens[i], test_bptree_search);
		ASSERT(!v, "insert");
	}

	printf("CHECK!!!!!\n");

	for (i = 1; i < BPTREE_SPLIT_ITT; i++) {
		i32 v;
		v = bptree_put(txn, keys[i], rand_key_lens[i], values[i],
			       rand_value_lens[i], test_bptree_search);
		ASSERT(v, "insertcheck");
	}

	bptree_close(tree);
	release(tree);
	release(txn);
	unlink(path);
}
*/
