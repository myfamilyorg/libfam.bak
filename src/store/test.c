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
#include <rng.H>
#include <test.H>

void test_bptree_search(BpTxn *txn, const void *key, uint16_t key_len,
			const BpTreeNode *node, BpTreeSearchResult *retval) {
	int i;
	/* Simplify for now, just use root node only */
	retval->found = false;

	/*
	printf("start loop node=%i entries=%x\n", bptree_node_id(txn, node),
	       node->data.leaf.entries);
	       */
	for (i = 0; i < node->num_entries; i++) {
		int v;
		uint16_t offset = node->data.leaf.entry_offsets[i];
		BpTreeEntry *entry =
		    (BpTreeEntry *)((uint8_t *)node->data.leaf.entries +
				    offset);
		const char *cmp_key =
		    (const char *)((uint8_t *)entry + sizeof(BpTreeEntry));
		uint16_t len =
		    key_len > entry->key_len ? entry->key_len : key_len;

		v = strcmpn(key, cmp_key, len);

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

Test(store1) {
	const char *path = "/tmp/store1.dat";
	int fd, i;
	BpTree *tree;
	BpTxn *txn;
	Rng rng;
	uint8_t keys[11][20];
	uint8_t values[11][20];
	uint8_t rand_key_lens[11];
	uint8_t rand_value_lens[11];
	uint8_t key[32] = {2};
	uint8_t iv[16] = {2};

	/*rng_init(&rng);*/
	rng_test_seed(&rng, key, iv);
	unlink(path);
	fd = file(path);

	fresize(fd, PAGE_SIZE * 16);
	close(fd);
	tree = bptree_open(path);
	ASSERT(tree, "tree");
	txn = bptxn_start(tree);

	for (i = 1; i < 11; i++) {
		uint8_t keybuf[20] = {0};
		uint8_t valuebuf[20] = {0};
		size_t klen = 0, vlen = 0;
		rng_gen(&rng, keybuf, 20);
		rng_gen(&rng, valuebuf, 20);
		klen = b64_encode(keybuf, 12, keys[i], 20);
		vlen = b64_encode(valuebuf, 12, values[i], 20);
		keys[i][klen] = 0;
		values[i][vlen] = 0;
		rng_gen(&rng, &klen, sizeof(size_t));
		rng_gen(&rng, &vlen, sizeof(size_t));
		rand_key_lens[i] = (klen % 10) + 1;
		rand_value_lens[i] = (vlen % 10) + 1;
		/*
		printf("key[%i]=%s,value[%i]=%s,klen=%i,vlen=%i\n", i, keys[i],
		       i, values[i], rand_key_lens[i], rand_value_lens[i]);
		       */
	}

	for (i = 1; i < 11; i++) {
		uint8_t buf[20];
		int v;
		rng_gen(&rng, buf, 20);
		v = bptree_put(txn, keys[i], rand_key_lens[i], values[i],
			       rand_value_lens[i], test_bptree_search);
		ASSERT(!v, "insert");
	}

	for (i = 1; i < 11; i++) {
		int v = bptree_put(txn, keys[i], rand_key_lens[i], values[i],
				   rand_value_lens[i], test_bptree_search);
		ASSERT(v, "insertcheck");
	}

	bptree_close(tree);
	release(tree);
	release(txn);
	unlink(path);
}
