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
