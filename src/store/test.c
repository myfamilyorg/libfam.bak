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
#include <bptree_prim.H>
#include <storage.H>
#include <test.H>

Test(storage1) {
	const u8 *path = "/tmp/storage1.dat";
	u8 buf[100];
	i64 v;
	u64 seqno;
	i32 fd;
	i32 wakeups[2];
	Env *e1;
	u64 n;

	unlink(path);
	fd = file(path);
	fresize(fd, NODE_SIZE * 16);
	close(fd);

	e1 = env_open(path);
	ASSERT_EQ(env_root(e1), 0, "root=0");
	seqno = env_root_seqno(e1);
	env_set_root(e1, seqno, 10);
	ASSERT_EQ(seqno + 1, env_root_seqno(e1), "seqno+1");
	ASSERT_EQ(env_root(e1), 10, "root=10");
	seqno = env_root_seqno(e1);
	env_set_root(e1, seqno, 12);
	ASSERT_EQ(seqno + 1, env_root_seqno(e1), "seqno+1");
	ASSERT_EQ(env_root(e1), 12, "root=12");
	seqno = env_root_seqno(e1);
	env_set_root(e1, seqno, 11);
	ASSERT_EQ(seqno + 1, env_root_seqno(e1), "seqno+1");
	ASSERT_EQ(env_root(e1), 11, "root=11");

	ASSERT(!pipe(wakeups), "pipe");

	v = env_register_notification(e1, wakeups[1]);
	ASSERT_EQ(read(wakeups[0], buf, 100), 1, "read=1");
	ASSERT_EQ(v + 1, env_counter(e1), "v+1=env_counter");

	v = env_register_notification(e1, wakeups[1]);
	ASSERT_EQ(read(wakeups[0], buf, 100), 1, "read=1");
	ASSERT_EQ(v + 1, env_counter(e1), "v+1=env_counter");

	n = env_alloc(e1);
	env_release(e1, n);
	/*env_release(e1, n);*/

	env_close(e1);
	release(e1);
	unlink(path);
}

#define ITT 5

Test(storage2) {
	const u8 *path = "/tmp/storage2.dat";
	u64 ptrs[ITT];
	i32 fd, i;
	Env *e1;

	unlink(path);
	fd = file(path);
	fresize(fd, NODE_SIZE * 8);
	close(fd);

	e1 = env_open(path);
	ASSERT_EQ(env_root(e1), 0, "root=0");
	env_set_root(e1, env_root_seqno(e1), 4);
	ASSERT_EQ(env_root(e1), 4, "root=4");

	for (i = 0; i < ITT; i++) {
		ptrs[i] = env_alloc(e1);
		ASSERT(ptrs[i], "ptrs");
	}
	ASSERT(!env_alloc(e1), "NULL");

	for (i = 0; i < ITT; i++) {
		env_release(e1, ptrs[i]);
	}

	env_close(e1);
	release(e1);
	unlink(path);
}

Test(bptree_prim1) {
	BpTreeNode node1, node2, node3, node4;
	BpTreeItem item1;
	const u8 *tmp;
	ASSERT(!bptree_prim_init_node(&node1, 4, false), "node1_init");
	ASSERT_EQ(bptree_prim_get_parent_id(&node1), 4, "parent");
	ASSERT_EQ(bptree_prim_num_entries(&node1), 0, "num_entries=0");

	item1.key_len = 2;
	item1.value_len = 1;
	item1.is_overflow = false;
	item1.vardata.kv.key = "ab";
	item1.vardata.kv.value = "z";

	ASSERT_EQ(bptree_prim_num_entries(&node1), 0, "num_entries=0");
	ASSERT_EQ(bptree_prim_used_bytes(&node1), 0, "used_bytes=0");

	ASSERT(!bptree_prim_insert_leaf_entry(&node1, 0, &item1),
	       "bptree_prim");
	ASSERT_EQ(bptree_prim_num_entries(&node1), 1, "num_entries=1");
	ASSERT_EQ(bptree_prim_used_bytes(&node1), 11,
		  "used_bytes=2+1+8(SizeofDataStructure)");
	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 2, "key_len=2");
	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 1, "value_len=1");
	tmp = bptree_prim_key(&node1, 0);
	ASSERT_EQ(tmp[0], 'a', "a");
	ASSERT_EQ(tmp[1], 'b', "b");
	tmp = bptree_prim_value(&node1, 0);
	ASSERT_EQ(tmp[0], 'z', "z");

	ASSERT(bptree_prim_is_copy(&node1), "is_copy");
	ASSERT(!bptree_prim_is_internal(&node1), "is_internal");

	bptree_prim_unset_copy(&node1);
	ASSERT(!bptree_prim_is_copy(&node1), "is_copy");
	bptree_prim_set_aux(&node1, 1234);
	ASSERT_EQ(bptree_prim_get_aux(&node1), 1234, "aux=1234");

	item1.key_len = 4;
	item1.value_len = 3;
	item1.is_overflow = false;
	item1.vardata.kv.key = "xxxx";
	item1.vardata.kv.value = "zab";

	/* Can't inseert to a non copy */
	ASSERT(bptree_prim_insert_leaf_entry(&node1, 0, &item1), "insert2");

	ASSERT(!bptree_prim_init_node(&node2, 1, false), "node2_init");
	ASSERT(!bptree_prim_insert_leaf_entry(&node2, 0, &item1),
	       "insert2_success");

	item1.key_len = 3;
	item1.value_len = 2;
	item1.is_overflow = false;
	item1.vardata.kv.key = "xyz";
	item1.vardata.kv.value = "aa";

	ASSERT(!bptree_prim_insert_leaf_entry(&node2, 0, &item1),
	       "insert2_success");

	item1.key_len = 3;
	item1.value_len = 1;
	item1.is_overflow = false;
	item1.vardata.kv.key = "aaa";
	item1.vardata.kv.value = "v";

	ASSERT(!bptree_prim_insert_leaf_entry(&node2, 1, &item1),
	       "insert3_success");

	ASSERT_EQ(bptree_prim_num_entries(&node2), 3, "ent=3");
	ASSERT_EQ(bptree_prim_key_len(&node2, 0), 3, "0key_len=3");
	ASSERT_EQ(bptree_prim_key_len(&node2, 1), 3, "1key_len=3");
	ASSERT_EQ(bptree_prim_key_len(&node2, 2), 4, "2key_len=4");
	ASSERT_EQ(bptree_prim_value_len(&node2, 0), 2, "0v_len=2");
	ASSERT_EQ(bptree_prim_value_len(&node2, 1), 1, "1v_len=1");
	ASSERT_EQ(bptree_prim_value_len(&node2, 2), 3, "2v_len=3");
	ASSERT(!strcmpn(bptree_prim_key(&node2, 0), "xyz", 3), "key0");
	ASSERT(!strcmpn(bptree_prim_key(&node2, 1), "aaa", 3), "key1");
	ASSERT(!strcmpn(bptree_prim_key(&node2, 2), "xxxx", 4), "key2");
	ASSERT(!strcmpn(bptree_prim_value(&node2, 0), "aa", 2), "value0");
	ASSERT(!strcmpn(bptree_prim_value(&node2, 1), "v", 1), "value1");
	ASSERT(!strcmpn(bptree_prim_value(&node2, 2), "zab", 3), "value2");

	item1.key_len = 10;
	item1.value_len = 10;
	item1.is_overflow = false;
	item1.vardata.kv.key = "0123456789";
	item1.vardata.kv.value = "abcdefghij";

	ASSERT(!bptree_prim_insert_leaf_entry(&node2, 1, &item1),
	       "insert4_success");

	ASSERT_EQ(bptree_prim_num_entries(&node2), 4, "ent=3");
	ASSERT_EQ(bptree_prim_key_len(&node2, 0), 3, "0key_len=3");
	ASSERT_EQ(bptree_prim_key_len(&node2, 2), 3, "2key_len=3");
	ASSERT_EQ(bptree_prim_key_len(&node2, 3), 4, "3key_len=4");
	ASSERT_EQ(bptree_prim_value_len(&node2, 0), 2, "0v_len=2");
	ASSERT_EQ(bptree_prim_value_len(&node2, 2), 1, "2v_len=1");
	ASSERT_EQ(bptree_prim_value_len(&node2, 3), 3, "3v_len=3");
	ASSERT(!strcmpn(bptree_prim_key(&node2, 0), "xyz", 3), "key0");
	ASSERT(!strcmpn(bptree_prim_key(&node2, 2), "aaa", 3), "key2");
	ASSERT(!strcmpn(bptree_prim_key(&node2, 3), "xxxx", 4), "key3");
	ASSERT(!strcmpn(bptree_prim_value(&node2, 0), "aa", 2), "value0");
	ASSERT(!strcmpn(bptree_prim_value(&node2, 2), "v", 1), "value2");
	ASSERT(!strcmpn(bptree_prim_value(&node2, 3), "zab", 3), "value3");

	/* Check new '1' */
	ASSERT_EQ(bptree_prim_key_len(&node2, 1), 10, "1key_len=10");
	ASSERT_EQ(bptree_prim_value_len(&node2, 1), 10, "1value_len=10");
	ASSERT(!strcmpn(bptree_prim_key(&node2, 1), "0123456789", 10), "key1");
	ASSERT(!strcmpn(bptree_prim_value(&node2, 1), "abcdefghij", 10),
	       "value1");

	ASSERT(!bptree_prim_init_node(&node3, 1, false), "node3_init");
	item1.key_len = 3;
	item1.value_len = 3;
	item1.is_overflow = false;
	item1.vardata.kv.key = "111";
	item1.vardata.kv.value = "abc";

	ASSERT(!bptree_prim_insert_leaf_entry(&node3, 0, &item1),
	       "insert0_success");

	item1.key_len = 4;
	item1.value_len = 4;
	item1.is_overflow = false;
	item1.vardata.kv.key = "2222";
	item1.vardata.kv.value = "abcd";

	ASSERT(!bptree_prim_insert_leaf_entry(&node3, 1, &item1),
	       "insert1_success");

	item1.key_len = 5;
	item1.value_len = 5;
	item1.is_overflow = false;
	item1.vardata.kv.key = "33333";
	item1.vardata.kv.value = "abcde";

	ASSERT(!bptree_prim_insert_leaf_entry(&node3, 2, &item1),
	       "insert1_success");

	item1.key_len = 6;
	item1.value_len = 6;
	item1.is_overflow = false;
	item1.vardata.kv.key = "444444";
	item1.vardata.kv.value = "abcdef";

	ASSERT(!bptree_prim_insert_leaf_entry(&node3, 3, &item1),
	       "insert1_success");

	ASSERT_EQ(bptree_prim_key_len(&node3, 0), 3, "0key_len=3");
	ASSERT_EQ(bptree_prim_key_len(&node3, 1), 4, "1key_len=4");
	ASSERT_EQ(bptree_prim_key_len(&node3, 2), 5, "2key_len=5");
	ASSERT_EQ(bptree_prim_key_len(&node3, 3), 6, "3key_len=6");
	ASSERT_EQ(bptree_prim_num_entries(&node3), 4, "nument");

	ASSERT(!bptree_prim_init_node(&node4, 1, false), "node4_init");
	ASSERT_EQ(bptree_prim_num_entries(&node4), 0, "nument");

	ASSERT(!bptree_prim_move_node_entries(&node4, 0, &node3, 2, 2),
	       "move_node");

	ASSERT_EQ(bptree_prim_num_entries(&node3), 2, "nument");
	ASSERT_EQ(bptree_prim_num_entries(&node4), 2, "nument");

	ASSERT_EQ(bptree_prim_key_len(&node3, 0), 3, "0key_len=3");
	ASSERT_EQ(bptree_prim_key_len(&node3, 1), 4, "1key_len=4");
	ASSERT_EQ(bptree_prim_value_len(&node3, 0), 3, "0value_len=3");
	ASSERT_EQ(bptree_prim_value_len(&node3, 1), 4, "1value_len=4");
	ASSERT(!strcmpn(bptree_prim_key(&node3, 0), "111", 3), "key0");
	ASSERT(!strcmpn(bptree_prim_key(&node3, 1), "2222", 4), "key1");
	ASSERT(!strcmpn(bptree_prim_value(&node3, 0), "abc", 3), "value0");
	ASSERT(!strcmpn(bptree_prim_value(&node3, 1), "abcd", 4), "value1");

	ASSERT_EQ(bptree_prim_key_len(&node4, 0), 5, "0key_len=5");
	ASSERT_EQ(bptree_prim_key_len(&node4, 1), 6, "1key_len=6");
	ASSERT_EQ(bptree_prim_value_len(&node4, 0), 5, "0value_len=5");
	ASSERT_EQ(bptree_prim_value_len(&node4, 1), 6, "1value_len=6");
	ASSERT(!strcmpn(bptree_prim_key(&node4, 0), "33333", 5), "key0");
	ASSERT(!strcmpn(bptree_prim_key(&node4, 1), "444444", 6), "key1");
	ASSERT(!strcmpn(bptree_prim_value(&node4, 0), "abcde", 5), "value0");
	ASSERT(!strcmpn(bptree_prim_value(&node4, 1), "abcdef", 6), "value1");
	ASSERT_EQ(bptree_prim_used_bytes(&node3), 30, "used_bytes=30");
	ASSERT_EQ(bptree_prim_used_bytes(&node4), 38, "used_bytes=38");
}

Test(bptree_prim2) {
	/*BpTreeNode node1 = {0}, node2 = {0}*/
	/*, node3 = {0}, node4 = {0}*/ /*;*/
	BpTreeNode node1, node2, __attribute__((unused)) node3,
	    __attribute__((unused)) node4;
	BpTreeItem item1;

	/* Initialize node1 (source node) with parent_id=5, leaf node */
	ASSERT(!bptree_prim_init_node(&node1, 5, false), "node1_init");
	ASSERT_EQ(bptree_prim_get_parent_id(&node1), 5, "parent=5");
	ASSERT_EQ(bptree_prim_num_entries(&node1), 0, "num_entries=0");
	ASSERT_EQ(bptree_prim_used_bytes(&node1), 0, "used_bytes=0");
	ASSERT(bptree_prim_is_copy(&node1), "is_copy");
	ASSERT(!bptree_prim_is_internal(&node1), "is_internal");

	/* Insert entries into node1: ("aaa", "xyz"), ("bbb", "uvw"), ("ccc",
	 * "rst"), ("dddd", "pqrs") */
	item1.is_overflow = false;
	item1.key_len = 3;
	item1.value_len = 3;
	item1.vardata.kv.key = "aaa";
	item1.vardata.kv.value = "xyz";
	ASSERT(!bptree_prim_insert_leaf_entry(&node1, 0, &item1), "insert1");

	item1.vardata.kv.key = "bbb";
	item1.vardata.kv.value = "uvw";
	ASSERT(!bptree_prim_insert_leaf_entry(&node1, 1, &item1), "insert2");

	item1.vardata.kv.key = "ccc";
	item1.vardata.kv.value = "rst";
	ASSERT(!bptree_prim_insert_leaf_entry(&node1, 2, &item1), "insert3");

	item1.key_len = 4;
	item1.value_len = 4;
	item1.vardata.kv.key = "dddd";
	item1.vardata.kv.value = "pqrs";
	ASSERT(!bptree_prim_insert_leaf_entry(&node1, 3, &item1), "insert4");

	/* Verify node1 state: 3*(3+3+8) + (4+4+8) = 58 bytes */
	ASSERT_EQ(bptree_prim_num_entries(&node1), 4, "node1_num_entries=4");
	ASSERT_EQ(bptree_prim_used_bytes(&node1), 58, "node1_used_bytes=58");
	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 3, "key0_len=3");
	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 3, "value0_len=3");
	ASSERT_EQ(bptree_prim_key_len(&node1, 3), 4, "key3_len=4");
	ASSERT_EQ(bptree_prim_value_len(&node1, 3), 4, "value3_len=4");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "aaa", 3), "key0=aaa");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 0), "xyz", 3), "value0=xyz");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 3), "dddd", 4), "key3=dddd");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 3), "pqrs", 4),
	       "value3=pqrs");

	/* Initialize node2 (destination node) */
	ASSERT(!bptree_prim_init_node(&node2, 6, false), "node2_init");
	ASSERT_EQ(bptree_prim_num_entries(&node2), 0, "node2_num_entries=0");

	/* Move two entries from start of node1 (indices 0,1: "aaa", "bbb") to
	 * node2 */
	ASSERT(!bptree_prim_move_node_entries(&node2, 0, &node1, 0, 2),
	       "move_start");

	/* Verify node1: should have "ccc", "dddd" (3+3+8 + 4+4+8 = 30 bytes) */
	ASSERT_EQ(bptree_prim_num_entries(&node1), 2, "node1_num_entries=2");
	ASSERT_EQ(bptree_prim_used_bytes(&node1), 30, "node1_used_bytes=30");
	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 3, "node1_key0_len=3");
	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 4, "node1_key1_len=4");

	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 3, "node1_value0_len=3");
	ASSERT_EQ(bptree_prim_value_len(&node1, 1), 4, "node1_value1_len=4");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "ccc", 3),
	       "node1_key0=ccc");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "dddd", 4),
	       "node1_key1=dddd");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 0), "rst", 3),
	       "node1_value0=rst");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 1), "pqrs", 4),
	       "node1_value1=pqrs");

	ASSERT_EQ(bptree_prim_num_entries(&node2), 2, "node2_num_entries=2");
	ASSERT_EQ(bptree_prim_used_bytes(&node2), 28, "node2_used_bytes=28");
	ASSERT_EQ(bptree_prim_key_len(&node2, 0), 3, "node2_key0_len=3");
	ASSERT_EQ(bptree_prim_key_len(&node2, 1), 3, "node2_key1_len=3");
	ASSERT_EQ(bptree_prim_value_len(&node2, 0), 3, "node2_value0_len=3");
	ASSERT_EQ(bptree_prim_value_len(&node2, 1), 3, "node2_value1_len=3");
	ASSERT(!strcmpn(bptree_prim_key(&node2, 0), "aaa", 3),
	       "node2_key0=aaa");
	ASSERT(!strcmpn(bptree_prim_key(&node2, 1), "bbb", 3),
	       "node2_key1=bbb");
	ASSERT(!strcmpn(bptree_prim_value(&node2, 0), "xyz", 3),
	       "node2_value0=xyz");
	ASSERT(!strcmpn(bptree_prim_value(&node2, 1), "uvw", 3),
	       "node2_value1=uvw");

	ASSERT(!bptree_prim_move_node_entries(&node2, 2, &node1, 0, 0),
	       "move_zero");
	ASSERT_EQ(bptree_prim_num_entries(&node2), 2, "node2_num_entries=2");
	ASSERT_EQ(bptree_prim_num_entries(&node1), 2, "node1_num_entries=2");

	ASSERT(bptree_prim_move_node_entries(&node2, 2, &node1, 2, 1),
	       "move_invalid_src");

	ASSERT_EQ(bptree_prim_num_entries(&node2), 2, "node2_num_entries=2");

	ASSERT(bptree_prim_move_node_entries(&node2, 3, &node1, 0, 1),
	       "move_invalid_dst");

	ASSERT_EQ(bptree_prim_num_entries(&node2), 2, "node2_num_entries=2");

	ASSERT(!bptree_prim_move_node_entries(&node2, 2, &node1, 0, 2),
	       "move_all");

	ASSERT_EQ(bptree_prim_num_entries(&node1), 0, "node1_num_entries=0");
	ASSERT_EQ(bptree_prim_used_bytes(&node1), 0, "node1_used_bytes=0");
	ASSERT_EQ(bptree_prim_num_entries(&node2), 4, "node2_num_entries=4");
	ASSERT_EQ(bptree_prim_used_bytes(&node2), 58, "node2_used_bytes=58");
	ASSERT(!strcmpn(bptree_prim_key(&node2, 2), "ccc", 3),
	       "node2_key2=ccc");
	ASSERT(!strcmpn(bptree_prim_key(&node2, 3), "dddd", 4),
	       "node2_key3=dddd");
	ASSERT(!strcmpn(bptree_prim_value(&node2, 2), "rst", 3),
	       "node2_value2=rst");
	ASSERT(!strcmpn(bptree_prim_value(&node2, 3), "pqrs", 4),
	       "node2_value3=pqrs");

	ASSERT(!bptree_prim_init_node(&node3, 7, false), "node3_init");
	item1.key_len = 3;
	item1.value_len = 3;
	item1.vardata.kv.key = "eee";
	item1.vardata.kv.value = "fgh";
	ASSERT(!bptree_prim_insert_leaf_entry(&node3, 0, &item1),
	       "insert_node3");
	bptree_prim_unset_copy(&node3);
	ASSERT(!bptree_prim_is_copy(&node3), "node3_not_copy");
	ASSERT(bptree_prim_move_node_entries(&node1, 0, &node3, 0, 1),
	       "move_non_copy_src");
	ASSERT_EQ(bptree_prim_num_entries(&node1), 0, "node1_num_entries=0");
	ASSERT_EQ(bptree_prim_num_entries(&node3), 1, "node3_num_entries=1");

	ASSERT(!bptree_prim_init_node(&node4, 8, false), "node4_init");
	item1.key_len = 15;
	item1.value_len = 15;
	item1.vardata.kv.key = "large_key_123456";
	item1.vardata.kv.value = "large_val_abcdef";
	ASSERT(!bptree_prim_insert_leaf_entry(&node4, 0, &item1),
	       "insert_large1");
	ASSERT(!bptree_prim_insert_leaf_entry(&node4, 1, &item1),
	       "insert_large2");
	ASSERT_EQ(bptree_prim_used_bytes(&node4), 76, "node4_used_bytes=76");

	ASSERT(!bptree_prim_init_node(&node3, 7, false), "node3_reinit");
	item1.key_len = 3;
	item1.value_len = 3;
	item1.vardata.kv.key = "eee";
	item1.vardata.kv.value = "fgh";
	ASSERT(!bptree_prim_insert_leaf_entry(&node3, 0, &item1),
	       "insert_node3");

	ASSERT(!bptree_prim_move_node_entries(&node4, 2, &node3, 0, 1),
	       "move_to_full");
	ASSERT_EQ(bptree_prim_num_entries(&node4), 3, "node4_num_entries=3");
	ASSERT_EQ(bptree_prim_used_bytes(&node4), 90, "node4_used_bytes=90");
	ASSERT(!strcmpn(bptree_prim_key(&node4, 2), "eee", 3),
	       "node4_key2=eee");
	ASSERT(!strcmpn(bptree_prim_value(&node4, 2), "fgh", 3),
	       "node4_value2=fgh");
	ASSERT_EQ(bptree_prim_num_entries(&node3), 0, "node3_num_entries=0");

	item1.key_len = 20;
	item1.value_len = 20;
	item1.vardata.kv.key = "too_large_key_12345678";
	item1.vardata.kv.value = "too_large_val_abcdefgh";
	ASSERT(!bptree_prim_insert_leaf_entry(&node3, 0, &item1),
	       "insert_node3_large");
}
