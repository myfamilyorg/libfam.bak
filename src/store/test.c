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
#include <bptree_prim.H>
#include <error.H>
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
	BpTreeNode node1;
	BpTreeItem item1;

	ASSERT(!bptree_prim_init_node(&node1, 4, false), "node1_init");
	ASSERT_EQ(bptree_prim_parent_id(&node1), 4, "parent");
	ASSERT_EQ(bptree_prim_num_entries(&node1), 0, "num_entries=0");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 1;
	item1.key = "ab";
	item1.vardata.kv.value = "z";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert ab");
	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 2, "key_len=2");
	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 1, "value_len=1");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "ab", 2), "key=ab");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 0), "z", 1), "value=z");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 3;
	item1.key = "def";
	item1.vardata.kv.value = "xab";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert def");
	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 3, "key_len=3");
	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 3, "value_len=3");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "def", 3), "key=def");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 0), "xab", 3), "value=xab");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 2, "key_len=2");
	ASSERT_EQ(bptree_prim_value_len(&node1, 1), 1, "value_len=1");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "ab", 2), "key=ab");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 1), "z", 1), "value=z");

	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 5;
	item1.key = "ghij";
	item1.vardata.kv.value = "ddddd";
	ASSERT(!bptree_prim_insert_entry(&node1, 1, &item1), "insert def");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 3, "key_len=3");
	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 3, "value_len=3");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "def", 3), "key=def");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 0), "xab", 3), "value=xab");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 4, "key_len=4");
	ASSERT_EQ(bptree_prim_value_len(&node1, 1), 5, "value_len=5");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "ghij", 4), "key=ghij");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 1), "ddddd", 5),
	       "value=ddddd");

	ASSERT_EQ(bptree_prim_key_len(&node1, 2), 2, "key_len=2");
	ASSERT_EQ(bptree_prim_value_len(&node1, 2), 1, "value_len=1");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 2), "ab", 2), "key=ab");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 2), "z", 1), "value=z");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_OVERFLOW;
	item1.vardata.overflow.value_len = 4;
	item1.vardata.overflow.overflow_start = 123;
	item1.vardata.overflow.overflow_end = 456;
	item1.key = "x1";

	ASSERT(!bptree_prim_insert_entry(&node1, 1, &item1), "insert overflow");
	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 3, "key_len=3");
	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 3, "value_len=3");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "def", 3), "key=def");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 0), "xab", 3), "value=xab");
	ASSERT_EQ(bptree_prim_is_overflow(&node1, 0), false, "is overflow");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 2, "key_len=2");
	ASSERT_EQ(bptree_prim_value_len(&node1, 1), 4, "value_len=4");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "x1", 2), "key=x1");
	ASSERT_EQ(bptree_prim_is_overflow(&node1, 1), true, "is overflow");
	ASSERT_EQ(bptree_prim_overflow_start(&node1, 1), 123, "start=123");
	ASSERT_EQ(bptree_prim_overflow_end(&node1, 1), 456, "end=456");

	ASSERT_EQ(bptree_prim_key_len(&node1, 2), 4, "key_len=4");
	ASSERT_EQ(bptree_prim_value_len(&node1, 2), 5, "value_len=5");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 2), "ghij", 4), "key=ghij");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 2), "ddddd", 5),
	       "value=ddddd");

	ASSERT_EQ(bptree_prim_key_len(&node1, 3), 2, "key_len=2");
	ASSERT_EQ(bptree_prim_value_len(&node1, 3), 1, "value_len=1");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 3), "ab", 2), "key=ab");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 3), "z", 1), "value=z");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_OVERFLOW;
	item1.vardata.overflow.value_len = 3939392;
	item1.vardata.overflow.overflow_start = 999;
	item1.vardata.overflow.overflow_end = 888;
	item1.key = "a99";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1),
	       "insert overflow2");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 3, "key_len=3");
	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 3939392,
		  "value_len=3939392");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "a99", 3), "key=a99");
	ASSERT_EQ(bptree_prim_is_overflow(&node1, 0), true, "is overflow");
	ASSERT_EQ(bptree_prim_overflow_start(&node1, 0), 999, "start=999");
	ASSERT_EQ(bptree_prim_overflow_end(&node1, 0), 888, "end=888");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 3, "key_len=3");
	ASSERT_EQ(bptree_prim_value_len(&node1, 1), 3, "value_len=3");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "def", 3), "key=def");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 1), "xab", 3), "value=xab");
	ASSERT_EQ(bptree_prim_is_overflow(&node1, 1), false, "is overflow");

	ASSERT_EQ(bptree_prim_key_len(&node1, 2), 2, "key_len=2");
	ASSERT_EQ(bptree_prim_value_len(&node1, 2), 4, "value_len=4");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 2), "x1", 2), "key=x1");
	ASSERT_EQ(bptree_prim_is_overflow(&node1, 2), true, "is overflow");
	ASSERT_EQ(bptree_prim_overflow_start(&node1, 2), 123, "start=123");
	ASSERT_EQ(bptree_prim_overflow_end(&node1, 2), 456, "end=456");

	ASSERT_EQ(bptree_prim_key_len(&node1, 3), 4, "key_len=4");
	ASSERT_EQ(bptree_prim_value_len(&node1, 3), 5, "value_len=5");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 3), "ghij", 4), "key=ghij");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 3), "ddddd", 5),
	       "value=ddddd");

	ASSERT_EQ(bptree_prim_key_len(&node1, 4), 2, "key_len=2");
	ASSERT_EQ(bptree_prim_value_len(&node1, 4), 1, "value_len=1");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 4), "ab", 2), "key=ab");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 4), "z", 1), "value=z");

	ASSERT_EQ(bptree_prim_num_entries(&node1), 5, "num_entries=5");
}

Test(bptree_prim2) {
	BpTreeNode node1;
	BpTreeItem item1;

	ASSERT(!bptree_prim_init_node(&node1, 7, true), "node1_init");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.vardata.internal.node_id = 777;
	item1.key = "a77";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1),
	       "insert internal1");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 3, "key_len=3");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "a77", 3), "key=a77");
	ASSERT_EQ(bptree_prim_node_id(&node1, 0), 777, "node_id=777");

	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.vardata.internal.node_id = 776;
	item1.key = "a760";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1),
	       "insert internal1");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 4, "key_len=4");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "a760", 4), "key=a760");
	ASSERT_EQ(bptree_prim_node_id(&node1, 0), 776, "node_id=776");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 3, "key_len=3");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "a77", 3), "key=a77");
	ASSERT_EQ(bptree_prim_node_id(&node1, 1), 777, "node_id=777");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.vardata.internal.node_id = 775;
	item1.key = "a9";

	ASSERT(!bptree_prim_insert_entry(&node1, 1, &item1),
	       "insert internal1");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 4, "key_len=4");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "a760", 4), "key=a760");
	ASSERT_EQ(bptree_prim_node_id(&node1, 0), 776, "node_id=776");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 2, "key_len=2");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "a9", 2), "key=a9");
	ASSERT_EQ(bptree_prim_node_id(&node1, 1), 775, "node_id=775");

	ASSERT_EQ(bptree_prim_key_len(&node1, 2), 3, "key_len=3");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 2), "a77", 3), "key=a77");
	ASSERT_EQ(bptree_prim_node_id(&node1, 2), 777, "node_id=777");
}

Test(bptree_prim3) {
	BpTreeNode node1, node2;
	BpTreeItem item1;

	ASSERT(!bptree_prim_init_node(&node1, 7, false), "node1_init");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 1;
	item1.key = "aa";
	item1.vardata.kv.value = "z";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv0");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 2;
	item1.key = "bbb";
	item1.vardata.kv.value = "zx";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 3;
	item1.key = "cccc";
	item1.vardata.kv.value = "9xa";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 1;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 5;
	item1.key = "d";
	item1.vardata.kv.value = "xxxxx";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv1");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 1, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 5, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "d", 1), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 0), "xxxxx", 5), "value");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 4, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 1), 3, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "cccc", 4), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 1), "9xa", 3), "value");

	ASSERT_EQ(bptree_prim_key_len(&node1, 2), 3, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 2), 2, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 2), "bbb", 3), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 2), "zx", 2), "value");

	ASSERT_EQ(bptree_prim_key_len(&node1, 3), 2, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 3), 1, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 3), "aa", 2), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 3), "z", 1), "value");

	ASSERT_EQ(bptree_prim_num_entries(&node1), 4, "num_entries");

	ASSERT(!bptree_prim_init_node(&node2, 0, false), "node2_init");
	ASSERT_EQ(bptree_prim_num_entries(&node2), 0, "num_entries");

	ASSERT(!bptree_prim_move_entries(&node2, 0, &node1, 1, 2), "move");

	ASSERT_EQ(bptree_prim_num_entries(&node1), 2, "num1");
	ASSERT_EQ(bptree_prim_num_entries(&node2), 2, "num2");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 1, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 5, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "d", 1), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 0), "xxxxx", 5), "value");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 2, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 1), 1, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "aa", 2), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 1), "z", 1), "value");

	ASSERT_EQ(bptree_prim_key_len(&node2, 0), 4, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node2, 0), 3, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node2, 0), "cccc", 4), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node2, 0), "9xa", 3), "value");

	ASSERT_EQ(bptree_prim_key_len(&node2, 1), 3, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node2, 1), 2, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node2, 1), "bbb", 3), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node2, 1), "zx", 2), "value");
}

Test(bptree_prim4) {
	BpTreeNode node1;
	BpTreeItem item1;

	ASSERT(!bptree_prim_init_node(&node1, 7, false), "node1_init");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 1;
	item1.key = "aa";
	item1.vardata.kv.value = "z";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv0");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 2;
	item1.key = "bbb";
	item1.vardata.kv.value = "zx";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 3;
	item1.key = "cccc";
	item1.vardata.kv.value = "9xa";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 1;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 5;
	item1.key = "d";
	item1.vardata.kv.value = "xxxxx";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv1");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 1, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 5, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "d", 1), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 0), "xxxxx", 5), "value");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 4, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 1), 3, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "cccc", 4), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 1), "9xa", 3), "value");

	ASSERT_EQ(bptree_prim_key_len(&node1, 2), 3, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 2), 2, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 2), "bbb", 3), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 2), "zx", 2), "value");

	ASSERT_EQ(bptree_prim_key_len(&node1, 3), 2, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 3), 1, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 3), "aa", 2), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 3), "z", 1), "value");

	ASSERT(!bptree_prim_delete_entry(&node1, 1), "delete");
	ASSERT_EQ(bptree_prim_num_entries(&node1), 3, "ent=3");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 1, "0key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 5, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "d", 1), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 0), "xxxxx", 5), "value");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 3, "1key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 1), 2, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "bbb", 3), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 1), "zx", 2), "value");

	ASSERT_EQ(bptree_prim_key_len(&node1, 2), 2, "2key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 2), 1, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 2), "aa", 2), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 2), "z", 1), "value");

	ASSERT(!bptree_prim_delete_entry(&node1, 2), "delete");
	ASSERT_EQ(bptree_prim_num_entries(&node1), 2, "ent=2");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 1, "0key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 5, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "d", 1), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 0), "xxxxx", 5), "value");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 3, "1key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 1), 2, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "bbb", 3), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 1), "zx", 2), "value");
}

Test(bptree_prim5) {
	BpTreeNode node1;
	BpTreeItem item1;

	ASSERT(!bptree_prim_init_node(&node1, 7, true), "node1_init");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.vardata.internal.node_id = 1;
	item1.key = "aa";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv0");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.key = "bbb";
	item1.vardata.internal.node_id = 2;

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.key = "cccc";
	item1.vardata.internal.node_id = 3;

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv2");

	item1.key_len = 1;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.key = "d";
	item1.vardata.internal.node_id = 4;

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv3");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 1, "key_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "d", 1), "key");
	ASSERT_EQ(bptree_prim_node_id(&node1, 0), 4, "node4");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 4, "key_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "cccc", 4), "key");
	ASSERT_EQ(bptree_prim_node_id(&node1, 1), 3, "node3");

	ASSERT_EQ(bptree_prim_key_len(&node1, 2), 3, "key_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 2), "bbb", 3), "key");
	ASSERT_EQ(bptree_prim_node_id(&node1, 2), 2, "node2");

	ASSERT_EQ(bptree_prim_key_len(&node1, 3), 2, "key_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 3), "aa", 2), "key");
	ASSERT_EQ(bptree_prim_node_id(&node1, 3), 1, "node1");

	ASSERT(!bptree_prim_delete_entry(&node1, 1), "delete");
	ASSERT_EQ(bptree_prim_num_entries(&node1), 3, "ent=3");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 1, "0key_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "d", 1), "key");
	ASSERT_EQ(bptree_prim_node_id(&node1, 0), 4, "node4");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 3, "1key_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "bbb", 3), "key");
	ASSERT_EQ(bptree_prim_node_id(&node1, 1), 2, "node2");

	ASSERT_EQ(bptree_prim_key_len(&node1, 2), 2, "2key_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 2), "aa", 2), "key");
	ASSERT_EQ(bptree_prim_node_id(&node1, 2), 1, "node1");

	ASSERT(!bptree_prim_delete_entry(&node1, 2), "delete");
	ASSERT_EQ(bptree_prim_num_entries(&node1), 2, "ent=2");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 1, "0key_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "d", 1), "key");
	ASSERT_EQ(bptree_prim_node_id(&node1, 0), 4, "node4");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 3, "1key_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "bbb", 3), "key");
	ASSERT_EQ(bptree_prim_node_id(&node1, 1), 2, "node2");
}

Test(bptree_prim6) {
	BpTreeNode node1;
	BpTreeItem item1;

	ASSERT(!bptree_prim_init_node(&node1, 7, false), "node1_init");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 1;
	item1.key = "aa";
	item1.vardata.kv.value = "z";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv0");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 2;
	item1.key = "bbb";
	item1.vardata.kv.value = "zx";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 3;
	item1.key = "cccc";
	item1.vardata.kv.value = "9xa";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 1;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 5;
	item1.key = "d";
	item1.vardata.kv.value = "xxxxx";

	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 10;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 10;
	item1.key = "0123456789";
	item1.vardata.kv.value = "abcdefghij";

	ASSERT(!bptree_prim_set_entry(&node1, 2, &item1), "set_entry");

	ASSERT_EQ(bptree_prim_key_len(&node1, 0), 1, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 0), 5, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 0), "d", 1), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 0), "xxxxx", 5), "value");

	ASSERT_EQ(bptree_prim_key_len(&node1, 1), 4, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 1), 3, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 1), "cccc", 4), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 1), "9xa", 3), "value");

	ASSERT_EQ(bptree_prim_key_len(&node1, 2), 10, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 2), 10, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 2), "0123456789", 10), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 2), "abcdefghij", 10),
	       "value");

	ASSERT_EQ(bptree_prim_key_len(&node1, 3), 2, "key_len");
	ASSERT_EQ(bptree_prim_value_len(&node1, 3), 1, "value_len");
	ASSERT(!strcmpn(bptree_prim_key(&node1, 3), "aa", 2), "key");
	ASSERT(!strcmpn(bptree_prim_value(&node1, 3), "z", 1), "value");
}

#define MAX_LEAF_ENTRIES ((u64)(NODE_SIZE / 32))
#define MAX_INTERNAL_ENTRIES (NODE_SIZE / 16)
#define INTERNAL_ARRAY_SIZE ((28 * NODE_SIZE - 1024) / 32)
#define LEAF_ARRAY_SIZE ((u64)((30 * NODE_SIZE - 1280) / 32))

Test(bptree_prim7) {
	BpTreeNode node1, node2;
	BpTreeItem item1;
	u8 buf[NODE_SIZE];
	u64 i;

	ASSERT(!bptree_prim_init_node(&node1, 7, false), "node1_init");
	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 5;
	item1.key = "abc";
	item1.vardata.kv.value = "xxxxx";
	ASSERT(!bptree_prim_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = LEAF_ARRAY_SIZE;
	item1.key = "def";
	item1.vardata.kv.value = buf;

	/* overflow */
	ASSERT(bptree_prim_insert_entry(&node1, 0, &item1), "insert kv2");

	ASSERT(!bptree_prim_init_node(&node2, 7, false), "node2_init");

	for (i = 0; i < MAX_LEAF_ENTRIES; i++) {
		u8 key_buf[128] = {0};
		i32 len = u128_to_string(key_buf, i);
		key_buf[len] = 0;
		item1.key_len = 3;
		item1.item_type = BPTREE_ITEM_TYPE_LEAF;
		item1.vardata.kv.value_len = 1;
		item1.key = key_buf;
		item1.vardata.kv.value = buf;

		ASSERT(!bptree_prim_insert_entry(&node2, i, &item1), "insertx");
	}

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = LEAF_ARRAY_SIZE;
	item1.key = "def";
	item1.vardata.kv.value = buf;

	ASSERT(bptree_prim_insert_entry(&node2, i, &item1), "insertx");
}

void test_bptree_search(BpTxn *txn, const void *key, u16 key_len,
			const BpTreeNode *node, BpTreeSearchResult *retval) {
	i32 i;
	u16 num_entries;
	retval->found = false;
	retval->levels = 0;

	while (bptree_prim_is_internal(node)) {
		u16 match_index = 0;
		u64 node_id;
		num_entries = bptree_prim_num_entries(node);

		for (i = 1; i < num_entries; i++) {
			i32 v;
			u16 len = bptree_prim_key_len(node, i);
			const u8 *cmp_key = bptree_prim_key(node, i);
			v = strcmpn(key, cmp_key, len);
			if (v < 0) break;
			match_index++;
		}

		node_id = bptree_prim_node_id(node, match_index);
		node = bptxn_get_node(txn, node_id);
		retval->parent_index[retval->levels++] = match_index;
	}
	num_entries = bptree_prim_num_entries(node);
	for (i = 0; i < num_entries; i++) {
		const u8 *cmp_key = bptree_prim_key(node, i);
		u16 len = bptree_prim_key_len(node, i);
		i32 v = strcmpn(key, cmp_key, len);

		if (v == 0 && key_len == len) {
			retval->found = true;
			break;
		}
		if (v < 0) break;
	}

	retval->node_id = bptree_node_id(txn, node);
	retval->key_index = i;
}

void __attribute__((unused)) print_node(BpTxn *txn, const BpTreeNode *node,
					u8 *prefix) {
	u64 node_id = bptree_node_id(txn, node);
	bool is_copy = bptree_prim_is_copy(node);
	if (bptree_prim_is_internal(node)) {
		u16 i;
		u16 num_entries = bptree_prim_num_entries(node);
		printf(
		    "%s\tInternal "
		    "num_entries=%i,used_bytes=%i,parent=%i,is_copy=%i,node_id="
		    "%i\n",
		    prefix, num_entries, bptree_prim_used_bytes(node),
		    bptree_prim_parent_id(node), is_copy, node_id);
		for (i = 0; i < num_entries; i++) {
			u8 tmp[NODE_SIZE];
			u16 key_len = bptree_prim_key_len(node, i);
			const u8 *key = bptree_prim_key(node, i);
			u64 node_id = bptree_prim_node_id(node, i);
			memcpy(tmp, key, key_len);
			tmp[key_len] = 0;
			printf(
			    "%s\t\tkey[%u(len=%u]='%s',node_id=%u,is_copy=%i\n",
			    prefix, i, key_len, tmp, node_id, is_copy);
		}
		for (i = 0; i < num_entries; i++) {
			u64 node_id = bptree_prim_node_id(node, i);
			BpTreeNode *next = bptxn_get_node(txn, node_id);
			printf("\n");
			print_node(txn, next, "\t");
		}
	} else {
		i32 i;
		u16 num_entries = bptree_prim_num_entries(node);
		u16 used_bytes = bptree_prim_used_bytes(node);
		u64 parent_id = bptree_prim_parent_id(node);

		printf(
		    "%s\tLeaf "
		    "num_entries=%i,used_bytes=%i,parent=%i,node_id="
		    "%i,is_copy=%i\n",
		    prefix, num_entries, used_bytes, parent_id, node_id,
		    is_copy);
		for (i = 0; i < num_entries; i++) {
			u8 tmp[NODE_SIZE];
			u16 key_len = bptree_prim_key_len(node, i);
			const u8 *key = bptree_prim_key(node, i);
			u32 value_len = bptree_prim_value_len(node, i);
			memcpy(tmp, key, key_len);
			tmp[key_len] = 0;

			printf(
			    "%s\t\tkey[%u (len=%u)]='%s',value_len=%"
			    "u\n",
			    prefix, i, key_len, tmp, value_len);
		}
	}
}

void __attribute__((unused)) print_tree(BpTxn *txn) {
	BpTreeNode __attribute__((unused)) *root = bptree_root(txn);
#ifdef DEBUG1
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
#endif /* DEBUG1 */
}

Test(simple_split) {
	const u8 *path = "/tmp/store1.dat";
	i32 fd, i, v, wakeups[2];
	u64 node_id;
	i64 num;
	u8 buf[100];
	BpTree *tree;
	BpTxn *txn;

	u8 key1[16384], key2[16384], key3[16384], key4[16384], key5[16384];
	u8 value1[16384], value2[16384], value3[16384], value4[16384],
	    value5[16384];

	Env *env;

	unlink(path);
	fd = file(path);
	fresize(fd, NODE_SIZE * 128);
	close(fd);
	env = env_open(path);

	tree = bptree_open(env);
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
	v = bptree_put(txn, key1, 16, value1, 76, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	v = bptree_put(txn, key2, 16, value2, 512, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	v = bptree_put(txn, key3, 16, value3, 100, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	v = bptree_put(txn, key4, 16, value4, 76, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	v = bptree_put(txn, key5, 16, value5, 12, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	key1[1] = 'b';
	v = bptree_put(txn, key1, 16, value1, 12, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	key4[1] = 'x';
	v = bptree_put(txn, key4, 16, value4, 12, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	key1[1] = 'c';
	v = bptree_put(txn, key1, 16, value1, 12, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	/*
	key1[1] = 'a';
	v = bptree_put(txn, key1, 16, value1, 12, test_bptree_search);
	ASSERT_EQ(v, -1, "v=-1");
	*/

	key1[2] = 'x';
	v = bptree_put(txn, key1, 16, value1, 7610, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	key1[1] = 'a';
	key1[2] = 'y';
	v = bptree_put(txn, key1, 16, value1, 7610, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	key5[2] = 'y';
	v = bptree_put(txn, key5, 16, value1, 7610, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	/*
	pipe(wakeups);
	node_id = bptree_node_id(txn, bptree_root(txn));
	ASSERT(node_id != env_root(env), "envroot != txnroot");
	num = bptxn_commit(txn, wakeups[1]);
	ASSERT_EQ(read(wakeups[0], buf, 100), 1, "read=1");
	ASSERT_EQ(node_id, env_root(env), "updated root");
	ASSERT(num < env_counter(env), "num<env_counter");
	*/

	pipe(wakeups);
	node_id = bptree_node_id(txn, bptree_root(txn));
	ASSERT(node_id != env_root(env), "envroot != txnroot");

	num = bptxn_commit(txn, wakeups[1]);
	ASSERT_EQ(read(wakeups[0], buf, 100), 1, "read=1");
	ASSERT_EQ(node_id, env_root(env), "updated root");
	ASSERT(num < env_counter(env), "num<env_counter");

	release(tree);
	release(txn);
	env_close(env);
	release(env);

	env = env_open(path);

	tree = bptree_open(env);
	ASSERT(tree, "tree");
	txn = bptxn_start(tree);
	ASSERT(txn, "txn");

	v = bptree_put(txn, key3, 16, value1, 10, test_bptree_search);
	ASSERT_EQ(v, -1, "v=-1");
	print_tree(txn);

	key3[1] = 'x';
	v = bptree_put(txn, key3, 16, value1, 10, test_bptree_search);
	ASSERT_EQ(v, 0, "v=0");
	print_tree(txn);

	node_id = bptree_node_id(txn, bptree_root(txn));
	ASSERT(node_id != env_root(env), "envroot != txnroot");

	num = bptxn_commit(txn, wakeups[1]);
	ASSERT_EQ(read(wakeups[0], buf, 100), 1, "read=1");
	ASSERT_EQ(node_id, env_root(env), "updated root");
	ASSERT(num < env_counter(env), "num<env_counter");

	release(tree);
	release(txn);
	env_close(env);
	release(env);

	env = env_open(path);

	tree = bptree_open(env);
	ASSERT(tree, "tree");
	txn = bptxn_start(tree);
	ASSERT(txn, "txn");

	print_tree(txn);
	v = bptree_put(txn, key3, 16, value1, 10, test_bptree_search);
	ASSERT_EQ(v, -1, "v=-1");
	print_tree(txn);

	bptxn_abort(txn);
	release(tree);
	release(txn);
	env_close(env);
	release(env);
	unlink(path);
}

