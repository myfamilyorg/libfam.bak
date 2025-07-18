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

#include <libfam/alloc.H>
#include <libfam/bptree_node.H>
#include <libfam/error.H>
#include <libfam/storage.H>
#include <libfam/test.H>

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

	v = env_register_on_sync(e1, wakeups[1]);
	ASSERT_EQ(read(wakeups[0], buf, 100), 1, "read=1");
	ASSERT_EQ(v + 1, env_counter(e1), "v+1=env_counter");

	v = env_register_on_sync(e1, wakeups[1]);
	ASSERT_EQ(read(wakeups[0], buf, 100), 1, "read=1");
	ASSERT_EQ(v + 1, env_counter(e1), "v+1=env_counter");

	n = env_alloc(e1);
	env_release(e1, n);

	env_close(e1);
	release(e1);
	unlink(path);

	close(wakeups[0]);
	close(wakeups[1]);
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
	ASSERT(env_base(e1), "e1");
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

Test(storage3) {
	const u8 *path = "/tmp/storage3.dat";
	i32 fd;
	Env *e1;

	unlink(path);
	fd = file(path);
	fresize(fd, 1);
	close(fd);

	e1 = env_open(path);
	ASSERT(!e1, "e1=NULL");

	unlink(path);
}

Test(storage4) {
	const u8 *path = "/tmp/storage4.dat";
	i32 fd;
	Env *e1;

	unlink(path);
	fd = file(path);
	fresize(fd, 8 * NODE_SIZE);
	close(fd);

	_debug_alloc_failure = 1;
	e1 = env_open(path);
	ASSERT(!e1, "e1=NULL");

	ASSERT_EQ(env_close(NULL), -1, "env_close NULL");
	ASSERT_EQ(env_set_root(NULL, 0, 0), -1, "set_root NULL");
	err = SUCCESS;
	ASSERT_EQ(env_alloc(NULL), 0, "env_alloc(NULL)=0");
	ASSERT_EQ(err, EINVAL, "einval");

	unlink(path);
}

Test(bptree_node1) {
	BpTreeNode node1;
	BpTreeItem item1;

	ASSERT(!bptree_node_init_node(&node1, 4, false), "node1_init");
	ASSERT_EQ(bptree_node_parent_id(&node1), 4, "parent");
	ASSERT_EQ(bptree_node_num_entries(&node1), 0, "num_entries=0");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 1;
	item1.key = "ab";
	item1.vardata.kv.value = "z";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert ab");
	ASSERT_EQ(bptree_node_key_len(&node1, 0), 2, "key_len=2");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 1, "value_len=1");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "ab", 2), "key=ab");
	ASSERT(!strcmpn(bptree_node_value(&node1, 0), "z", 1), "value=z");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 3;
	item1.key = "def";
	item1.vardata.kv.value = "xab";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert def");
	ASSERT_EQ(bptree_node_key_len(&node1, 0), 3, "key_len=3");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 3, "value_len=3");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "def", 3), "key=def");
	ASSERT(!strcmpn(bptree_node_value(&node1, 0), "xab", 3), "value=xab");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 2, "key_len=2");
	ASSERT_EQ(bptree_node_value_len(&node1, 1), 1, "value_len=1");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "ab", 2), "key=ab");
	ASSERT(!strcmpn(bptree_node_value(&node1, 1), "z", 1), "value=z");

	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 5;
	item1.key = "ghij";
	item1.vardata.kv.value = "ddddd";
	ASSERT(!bptree_node_insert_entry(&node1, 1, &item1), "insert def");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 3, "key_len=3");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 3, "value_len=3");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "def", 3), "key=def");
	ASSERT(!strcmpn(bptree_node_value(&node1, 0), "xab", 3), "value=xab");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 4, "key_len=4");
	ASSERT_EQ(bptree_node_value_len(&node1, 1), 5, "value_len=5");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "ghij", 4), "key=ghij");
	ASSERT(!strcmpn(bptree_node_value(&node1, 1), "ddddd", 5),
	       "value=ddddd");

	ASSERT_EQ(bptree_node_key_len(&node1, 2), 2, "key_len=2");
	ASSERT_EQ(bptree_node_value_len(&node1, 2), 1, "value_len=1");
	ASSERT(!strcmpn(bptree_node_key(&node1, 2), "ab", 2), "key=ab");
	ASSERT(!strcmpn(bptree_node_value(&node1, 2), "z", 1), "value=z");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_OVERFLOW;
	item1.vardata.overflow.value_len = 4;
	item1.vardata.overflow.overflow_start = 123;
	item1.vardata.overflow.overflow_end = 456;
	item1.key = "x1";

	ASSERT(!bptree_node_insert_entry(&node1, 1, &item1), "insert overflow");
	ASSERT_EQ(bptree_node_key_len(&node1, 0), 3, "key_len=3");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 3, "value_len=3");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "def", 3), "key=def");
	ASSERT(!strcmpn(bptree_node_value(&node1, 0), "xab", 3), "value=xab");
	ASSERT_EQ(bptree_node_is_overflow(&node1, 0), false, "is overflow");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 2, "key_len=2");
	ASSERT_EQ(bptree_node_value_len(&node1, 1), 4, "value_len=4");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "x1", 2), "key=x1");
	ASSERT_EQ(bptree_node_is_overflow(&node1, 1), true, "is overflow");
	ASSERT_EQ(bptree_node_overflow_start(&node1, 1), 123, "start=123");
	ASSERT_EQ(bptree_node_overflow_end(&node1, 1), 456, "end=456");

	ASSERT_EQ(bptree_node_key_len(&node1, 2), 4, "key_len=4");
	ASSERT_EQ(bptree_node_value_len(&node1, 2), 5, "value_len=5");
	ASSERT(!strcmpn(bptree_node_key(&node1, 2), "ghij", 4), "key=ghij");
	ASSERT(!strcmpn(bptree_node_value(&node1, 2), "ddddd", 5),
	       "value=ddddd");

	ASSERT_EQ(bptree_node_key_len(&node1, 3), 2, "key_len=2");
	ASSERT_EQ(bptree_node_value_len(&node1, 3), 1, "value_len=1");
	ASSERT(!strcmpn(bptree_node_key(&node1, 3), "ab", 2), "key=ab");
	ASSERT(!strcmpn(bptree_node_value(&node1, 3), "z", 1), "value=z");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_OVERFLOW;
	item1.vardata.overflow.value_len = 3939392;
	item1.vardata.overflow.overflow_start = 999;
	item1.vardata.overflow.overflow_end = 888;
	item1.key = "a99";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1),
	       "insert overflow2");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 3, "key_len=3");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 3939392,
		  "value_len=3939392");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "a99", 3), "key=a99");
	ASSERT_EQ(bptree_node_is_overflow(&node1, 0), true, "is overflow");
	ASSERT_EQ(bptree_node_overflow_start(&node1, 0), 999, "start=999");
	ASSERT_EQ(bptree_node_overflow_end(&node1, 0), 888, "end=888");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 3, "key_len=3");
	ASSERT_EQ(bptree_node_value_len(&node1, 1), 3, "value_len=3");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "def", 3), "key=def");
	ASSERT(!strcmpn(bptree_node_value(&node1, 1), "xab", 3), "value=xab");
	ASSERT_EQ(bptree_node_is_overflow(&node1, 1), false, "is overflow");

	ASSERT_EQ(bptree_node_key_len(&node1, 2), 2, "key_len=2");
	ASSERT_EQ(bptree_node_value_len(&node1, 2), 4, "value_len=4");
	ASSERT(!strcmpn(bptree_node_key(&node1, 2), "x1", 2), "key=x1");
	ASSERT_EQ(bptree_node_is_overflow(&node1, 2), true, "is overflow");
	ASSERT_EQ(bptree_node_overflow_start(&node1, 2), 123, "start=123");
	ASSERT_EQ(bptree_node_overflow_end(&node1, 2), 456, "end=456");

	ASSERT_EQ(bptree_node_key_len(&node1, 3), 4, "key_len=4");
	ASSERT_EQ(bptree_node_value_len(&node1, 3), 5, "value_len=5");
	ASSERT(!strcmpn(bptree_node_key(&node1, 3), "ghij", 4), "key=ghij");
	ASSERT(!strcmpn(bptree_node_value(&node1, 3), "ddddd", 5),
	       "value=ddddd");

	ASSERT_EQ(bptree_node_key_len(&node1, 4), 2, "key_len=2");
	ASSERT_EQ(bptree_node_value_len(&node1, 4), 1, "value_len=1");
	ASSERT(!strcmpn(bptree_node_key(&node1, 4), "ab", 2), "key=ab");
	ASSERT(!strcmpn(bptree_node_value(&node1, 4), "z", 1), "value=z");

	ASSERT_EQ(bptree_node_num_entries(&node1), 5, "num_entries=5");
}

Test(bptree_node2) {
	BpTreeNode node1;
	BpTreeItem item1;

	ASSERT(!bptree_node_init_node(&node1, 7, true), "node1_init");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.vardata.internal.node_id = 777;
	item1.key = "a77";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1),
	       "insert internal1");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 3, "key_len=3");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "a77", 3), "key=a77");
	ASSERT_EQ(bptree_node_node_id(&node1, 0), 777, "node_id=777");

	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.vardata.internal.node_id = 776;
	item1.key = "a760";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1),
	       "insert internal1");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 4, "key_len=4");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "a760", 4), "key=a760");
	ASSERT_EQ(bptree_node_node_id(&node1, 0), 776, "node_id=776");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 3, "key_len=3");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "a77", 3), "key=a77");
	ASSERT_EQ(bptree_node_node_id(&node1, 1), 777, "node_id=777");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.vardata.internal.node_id = 775;
	item1.key = "a9";

	ASSERT(!bptree_node_insert_entry(&node1, 1, &item1),
	       "insert internal1");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 4, "key_len=4");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "a760", 4), "key=a760");
	ASSERT_EQ(bptree_node_node_id(&node1, 0), 776, "node_id=776");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 2, "key_len=2");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "a9", 2), "key=a9");
	ASSERT_EQ(bptree_node_node_id(&node1, 1), 775, "node_id=775");

	ASSERT_EQ(bptree_node_key_len(&node1, 2), 3, "key_len=3");
	ASSERT(!strcmpn(bptree_node_key(&node1, 2), "a77", 3), "key=a77");
	ASSERT_EQ(bptree_node_node_id(&node1, 2), 777, "node_id=777");
}

Test(bptree_node3) {
	BpTreeNode node1, node2;
	BpTreeItem item1;

	ASSERT(!bptree_node_init_node(&node1, 7, false), "node1_init");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 1;
	item1.key = "aa";
	item1.vardata.kv.value = "z";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv0");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 2;
	item1.key = "bbb";
	item1.vardata.kv.value = "zx";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 3;
	item1.key = "cccc";
	item1.vardata.kv.value = "9xa";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 1;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 5;
	item1.key = "d";
	item1.vardata.kv.value = "xxxxx";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv1");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 1, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 5, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "d", 1), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 0), "xxxxx", 5), "value");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 4, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 1), 3, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "cccc", 4), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 1), "9xa", 3), "value");

	ASSERT_EQ(bptree_node_key_len(&node1, 2), 3, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 2), 2, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 2), "bbb", 3), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 2), "zx", 2), "value");

	ASSERT_EQ(bptree_node_key_len(&node1, 3), 2, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 3), 1, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 3), "aa", 2), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 3), "z", 1), "value");

	ASSERT_EQ(bptree_node_num_entries(&node1), 4, "num_entries");

	ASSERT(!bptree_node_init_node(&node2, 0, false), "node2_init");
	ASSERT_EQ(bptree_node_num_entries(&node2), 0, "num_entries");

	ASSERT(!bptree_node_move_entries(&node2, 0, &node1, 1, 2), "move");

	ASSERT_EQ(bptree_node_num_entries(&node1), 2, "num1");
	ASSERT_EQ(bptree_node_num_entries(&node2), 2, "num2");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 1, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 5, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "d", 1), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 0), "xxxxx", 5), "value");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 2, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 1), 1, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "aa", 2), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 1), "z", 1), "value");

	ASSERT_EQ(bptree_node_key_len(&node2, 0), 4, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node2, 0), 3, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node2, 0), "cccc", 4), "key");
	ASSERT(!strcmpn(bptree_node_value(&node2, 0), "9xa", 3), "value");

	ASSERT_EQ(bptree_node_key_len(&node2, 1), 3, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node2, 1), 2, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node2, 1), "bbb", 3), "key");
	ASSERT(!strcmpn(bptree_node_value(&node2, 1), "zx", 2), "value");
}

Test(bptree_node4) {
	BpTreeNode node1;
	BpTreeItem item1;

	ASSERT(!bptree_node_init_node(&node1, 7, false), "node1_init");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 1;
	item1.key = "aa";
	item1.vardata.kv.value = "z";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv0");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 2;
	item1.key = "bbb";
	item1.vardata.kv.value = "zx";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 3;
	item1.key = "cccc";
	item1.vardata.kv.value = "9xa";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 1;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 5;
	item1.key = "d";
	item1.vardata.kv.value = "xxxxx";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv1");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 1, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 5, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "d", 1), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 0), "xxxxx", 5), "value");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 4, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 1), 3, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "cccc", 4), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 1), "9xa", 3), "value");

	ASSERT_EQ(bptree_node_key_len(&node1, 2), 3, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 2), 2, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 2), "bbb", 3), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 2), "zx", 2), "value");

	ASSERT_EQ(bptree_node_key_len(&node1, 3), 2, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 3), 1, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 3), "aa", 2), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 3), "z", 1), "value");

	ASSERT(!bptree_node_delete_entry(&node1, 1), "delete");
	ASSERT_EQ(bptree_node_num_entries(&node1), 3, "ent=3");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 1, "0key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 5, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "d", 1), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 0), "xxxxx", 5), "value");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 3, "1key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 1), 2, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "bbb", 3), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 1), "zx", 2), "value");

	ASSERT_EQ(bptree_node_key_len(&node1, 2), 2, "2key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 2), 1, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 2), "aa", 2), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 2), "z", 1), "value");

	ASSERT(!bptree_node_delete_entry(&node1, 2), "delete");
	ASSERT_EQ(bptree_node_num_entries(&node1), 2, "ent=2");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 1, "0key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 5, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "d", 1), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 0), "xxxxx", 5), "value");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 3, "1key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 1), 2, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "bbb", 3), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 1), "zx", 2), "value");
}

Test(bptree_node5) {
	BpTreeNode node1;
	BpTreeItem item1;

	ASSERT(!bptree_node_init_node(&node1, 7, true), "node1_init");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.vardata.internal.node_id = 1;
	item1.key = "aa";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv0");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.key = "bbb";
	item1.vardata.internal.node_id = 2;

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.key = "cccc";
	item1.vardata.internal.node_id = 3;

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv2");

	item1.key_len = 1;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.key = "d";
	item1.vardata.internal.node_id = 4;

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv3");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 1, "key_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "d", 1), "key");
	ASSERT_EQ(bptree_node_node_id(&node1, 0), 4, "node4");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 4, "key_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "cccc", 4), "key");
	ASSERT_EQ(bptree_node_node_id(&node1, 1), 3, "node3");

	ASSERT_EQ(bptree_node_key_len(&node1, 2), 3, "key_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 2), "bbb", 3), "key");
	ASSERT_EQ(bptree_node_node_id(&node1, 2), 2, "node2");

	ASSERT_EQ(bptree_node_key_len(&node1, 3), 2, "key_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 3), "aa", 2), "key");
	ASSERT_EQ(bptree_node_node_id(&node1, 3), 1, "node1");

	ASSERT(!bptree_node_delete_entry(&node1, 1), "delete");
	ASSERT_EQ(bptree_node_num_entries(&node1), 3, "ent=3");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 1, "0key_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "d", 1), "key");
	ASSERT_EQ(bptree_node_node_id(&node1, 0), 4, "node4");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 3, "1key_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "bbb", 3), "key");
	ASSERT_EQ(bptree_node_node_id(&node1, 1), 2, "node2");

	ASSERT_EQ(bptree_node_key_len(&node1, 2), 2, "2key_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 2), "aa", 2), "key");
	ASSERT_EQ(bptree_node_node_id(&node1, 2), 1, "node1");

	ASSERT(!bptree_node_delete_entry(&node1, 2), "delete");
	ASSERT_EQ(bptree_node_num_entries(&node1), 2, "ent=2");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 1, "0key_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "d", 1), "key");
	ASSERT_EQ(bptree_node_node_id(&node1, 0), 4, "node4");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 3, "1key_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "bbb", 3), "key");
	ASSERT_EQ(bptree_node_node_id(&node1, 1), 2, "node2");
}

Test(bptree_node6) {
	BpTreeNode node1;
	BpTreeItem item1;

	ASSERT(!bptree_node_init_node(&node1, 7, false), "node1_init");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 1;
	item1.key = "aa";
	item1.vardata.kv.value = "z";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv0");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 2;
	item1.key = "bbb";
	item1.vardata.kv.value = "zx";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 3;
	item1.key = "cccc";
	item1.vardata.kv.value = "9xa";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 1;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 5;
	item1.key = "d";
	item1.vardata.kv.value = "xxxxx";

	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 10;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 10;
	item1.key = "0123456789";
	item1.vardata.kv.value = "abcdefghij";

	ASSERT(!bptree_node_set_entry(&node1, 2, &item1), "set_entry");

	ASSERT_EQ(bptree_node_key_len(&node1, 0), 1, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 5, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "d", 1), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 0), "xxxxx", 5), "value");

	ASSERT_EQ(bptree_node_key_len(&node1, 1), 4, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 1), 3, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "cccc", 4), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 1), "9xa", 3), "value");

	ASSERT_EQ(bptree_node_key_len(&node1, 2), 10, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 2), 10, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 2), "0123456789", 10), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 2), "abcdefghij", 10),
	       "value");

	ASSERT_EQ(bptree_node_key_len(&node1, 3), 2, "key_len");
	ASSERT_EQ(bptree_node_value_len(&node1, 3), 1, "value_len");
	ASSERT(!strcmpn(bptree_node_key(&node1, 3), "aa", 2), "key");
	ASSERT(!strcmpn(bptree_node_value(&node1, 3), "z", 1), "value");
}

Test(bptree_node7) {
	BpTreeNode node1, node2;
	BpTreeItem item1;
	u8 buf[NODE_SIZE];
	u64 i;

	ASSERT(!bptree_node_init_node(&node1, 7, false), "node1_init");
	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 5;
	item1.key = "abc";
	item1.vardata.kv.value = "xxxxx";
	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert kv1");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = LEAF_ARRAY_SIZE;
	item1.key = "def";
	item1.vardata.kv.value = buf;

	/* overflow */
	ASSERT(bptree_node_insert_entry(&node1, 0, &item1), "insert kv2");

	ASSERT(!bptree_node_init_node(&node2, 7, false), "node2_init");

	for (i = 0; i < MAX_LEAF_ENTRIES; i++) {
		u8 key_buf[128] = {0};
		i32 len = u128_to_string(key_buf, i);
		key_buf[len] = 0;
		item1.key_len = 3;
		item1.item_type = BPTREE_ITEM_TYPE_LEAF;
		item1.vardata.kv.value_len = 1;
		item1.key = key_buf;
		item1.vardata.kv.value = buf;

		ASSERT(!bptree_node_insert_entry(&node2, i, &item1), "insertx");
	}

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = LEAF_ARRAY_SIZE;
	item1.key = "def";
	item1.vardata.kv.value = buf;

	ASSERT(bptree_node_insert_entry(&node2, i, &item1), "insertx");
}

Test(bptree_node8) {
	BpTreeNode node1;
	BpTreeNode node2;
	BpTreeItem item1;

	/* Initialize leaf node */
	ASSERT(!bptree_node_init_node(&node1, 42, false), "node1_init");
	ASSERT(bptree_node_is_copy(&node1), "is_copy after init");
	ASSERT_EQ(bptree_node_parent_id(&node1), 42, "parent_id");
	ASSERT_EQ(bptree_node_aux(&node1), 0,
		  "aux default"); /* Assuming default is 0 */

	/* Set aux and parent */
	ASSERT(!bptree_node_set_aux(&node1, 12345), "set_aux");
	ASSERT_EQ(bptree_node_aux(&node1), 12345, "aux set");
	ASSERT(!bptree_node_set_parent(&node1, 99), "set_parent");
	ASSERT_EQ(bptree_node_parent_id(&node1), 99, "parent set");

	/* Insert a few entries */
	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 1;
	item1.key = "ab";
	item1.vardata.kv.value = "z";
	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert1");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 2;
	item1.key = "cde";
	item1.vardata.kv.value = "xy";
	ASSERT(!bptree_node_insert_entry(&node1, 1, &item1), "insert2");

	ASSERT_EQ(bptree_node_num_entries(&node1), 2, "num_entries=2");

	/* Copy to node2 */
	bptree_node_copy(&node2, &node1);
	ASSERT(bptree_node_is_copy(&node2), "node2 is_copy after copy");
	ASSERT_EQ(bptree_node_parent_id(&node2), 99, "parent copied");
	ASSERT_EQ(bptree_node_aux(&node2), 12345, "aux copied");
	ASSERT_EQ(bptree_node_num_entries(&node2), 2, "num_entries copied");
	ASSERT_EQ(bptree_node_key_len(&node2, 0), 2, "key_len0 copied");
	ASSERT(!strcmpn(bptree_node_key(&node2, 0), "ab", 2), "key0 copied");
	ASSERT_EQ(bptree_node_value_len(&node2, 0), 1, "value_len0 copied");
	ASSERT(!strcmpn(bptree_node_value(&node2, 0), "z", 1), "value0 copied");
	ASSERT_EQ(bptree_node_key_len(&node2, 1), 3, "key_len1 copied");
	ASSERT(!strcmpn(bptree_node_key(&node2, 1), "cde", 3), "key1 copied");
	ASSERT_EQ(bptree_node_value_len(&node2, 1), 2, "value_len1 copied");
	ASSERT(!strcmpn(bptree_node_value(&node2, 1), "xy", 2),
	       "value1 copied");

	/* Unset copy on node1 and test access denied */
	ASSERT(!bptree_node_unset_copy(&node1), "unset_copy");
	ASSERT(!bptree_node_is_copy(&node1), "not is_copy");
	ASSERT(bptree_node_set_parent(&node1, 100),
	       "set_parent denied"); /* Should fail with EACCES */
	ASSERT_EQ(bptree_node_parent_id(&node1), 99, "parent unchanged");
	ASSERT(bptree_node_insert_entry(&node1, 2, &item1),
	       "insert denied"); /* Should fail with EACCES */

	/* Set copy back */
	ASSERT(!bptree_node_set_copy(&node1), "set_copy");
	ASSERT(bptree_node_is_copy(&node1), "is_copy again");
	ASSERT(!bptree_node_set_parent(&node1, 100), "set_parent allowed");
	ASSERT_EQ(bptree_node_parent_id(&node1), 100, "parent updated");
}

Test(bptree_node9) {
	BpTreeNode node1;
	BpTreeNode internal_node;

	/* Initialize leaf node */
	ASSERT(!bptree_node_init_node(&node1, 5, false), "node1_init");
	ASSERT_EQ(bptree_node_next_leaf(&node1), 0,
		  "next_leaf default"); /* Assuming default is 0 */

	/* Set next leaf */
	ASSERT(!bptree_node_set_next_leaf(&node1, 6789), "set_next_leaf");
	ASSERT_EQ(bptree_node_next_leaf(&node1), 6789, "next_leaf set");

	/* Error cases for next_leaf */
	ASSERT(!bptree_node_init_node(&internal_node, 0, true),
	       "internal_init");
	ASSERT(bptree_node_set_next_leaf(&internal_node, 123),
	       "set_next_leaf on internal"); /* Should fail with EINVAL */
	ASSERT_EQ(bptree_node_next_leaf(&internal_node), 0,
		  "next_leaf unchanged on internal");

	/* Unset copy and test access denied */
	ASSERT(!bptree_node_unset_copy(&node1), "unset_copy");
	ASSERT(bptree_node_set_next_leaf(&node1, 9999),
	       "set_next_leaf denied"); /* Should fail with EACCES */
	ASSERT_EQ(bptree_node_next_leaf(&node1), 6789, "next_leaf unchanged");
}

Test(bptree_node10) {
	BpTreeNode node1;
	BpTreeNode node2;
	BpTreeNode leaf_node;
	BpTreeItem item1;

	/* Internal node move_entries */
	ASSERT(!bptree_node_init_node(&node1, 0, true), "node1_init internal");

	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.vardata.internal.node_id = 10;
	item1.key = "ab";
	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1),
	       "insert internal1");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.vardata.internal.node_id = 20;
	item1.key = "cde";
	ASSERT(!bptree_node_insert_entry(&node1, 1, &item1),
	       "insert internal2");

	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.vardata.internal.node_id = 30;
	item1.key = "fghi";
	ASSERT(!bptree_node_insert_entry(&node1, 2, &item1),
	       "insert internal3");

	ASSERT_EQ(bptree_node_num_entries(&node1), 3, "num_entries=3");

	ASSERT(!bptree_node_init_node(&node2, 0, true), "node2_init internal");
	ASSERT_EQ(bptree_node_num_entries(&node2), 0, "num_entries=0");

	/* Move 2 entries to empty node2 */
	ASSERT(!bptree_node_move_entries(&node2, 0, &node1, 1, 2),
	       "move internal");

	ASSERT_EQ(bptree_node_num_entries(&node1), 1, "node1 num=1");
	ASSERT_EQ(bptree_node_key_len(&node1, 0), 2, "node1 key_len0");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "ab", 2), "node1 key0");
	ASSERT_EQ(bptree_node_node_id(&node1, 0), 10, "node1 node_id0");

	ASSERT_EQ(bptree_node_num_entries(&node2), 2, "node2 num=2");
	ASSERT_EQ(bptree_node_key_len(&node2, 0), 3, "node2 key_len0");
	ASSERT(!strcmpn(bptree_node_key(&node2, 0), "cde", 3), "node2 key0");
	ASSERT_EQ(bptree_node_node_id(&node2, 0), 20, "node2 node_id0");
	ASSERT_EQ(bptree_node_key_len(&node2, 1), 4, "node2 key_len1");
	ASSERT(!strcmpn(bptree_node_key(&node2, 1), "fghi", 4), "node2 key1");
	ASSERT_EQ(bptree_node_node_id(&node2, 1), 30, "node2 node_id1");

	/* Move to non-empty destination */
	item1.key_len = 1;
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item1.vardata.internal.node_id = 40;
	item1.key = "j";
	ASSERT(!bptree_node_insert_entry(&node2, 0, &item1), "insert to node2");

	ASSERT_EQ(bptree_node_num_entries(&node2), 3,
		  "node2 num=3 after insert");

	ASSERT(!bptree_node_move_entries(&node2, 1, &node1, 0, 1),
	       "move to non-empty");

	ASSERT_EQ(bptree_node_num_entries(&node1), 0, "node1 num=0");
	ASSERT_EQ(bptree_node_num_entries(&node2), 4, "node2 num=4");
	ASSERT_EQ(bptree_node_key_len(&node2, 0), 1, "node2 key_len0");
	ASSERT(!strcmpn(bptree_node_key(&node2, 0), "j", 1), "node2 key0");
	ASSERT_EQ(bptree_node_node_id(&node2, 0), 40, "node2 node_id0");
	ASSERT_EQ(bptree_node_key_len(&node2, 1), 2, "node2 key_len1");
	ASSERT(!strcmpn(bptree_node_key(&node2, 1), "ab", 2), "node2 key1");
	ASSERT_EQ(bptree_node_node_id(&node2, 1), 10, "node2 node_id1");
	ASSERT_EQ(bptree_node_key_len(&node2, 2), 3, "node2 key_len2");
	ASSERT(!strcmpn(bptree_node_key(&node2, 2), "cde", 3), "node2 key2");
	ASSERT_EQ(bptree_node_node_id(&node2, 2), 20, "node2 node_id2");
	ASSERT_EQ(bptree_node_key_len(&node2, 3), 4, "node2 key_len3");
	ASSERT(!strcmpn(bptree_node_key(&node2, 3), "fghi", 4), "node2 key3");
	ASSERT_EQ(bptree_node_node_id(&node2, 3), 30, "node2 node_id3");

	/* Error cases for move_entries */
	ASSERT(bptree_node_move_entries(&node2, 0, &node1, 0, 1),
	       "move from empty src"); /* EINVAL */
	ASSERT(bptree_node_move_entries(&node1, 0, &node2, 5, 1),
	       "invalid src index"); /* EINVAL */
	ASSERT(bptree_node_move_entries(&node1, MAX_INTERNAL_ENTRIES, &node2, 0,
					1),
	       "invalid dst index"); /* EOVERFLOW */
	ASSERT(!bptree_node_init_node(&leaf_node, 0, false), "leaf_init");
	ASSERT(bptree_node_move_entries(&node2, 0, &leaf_node, 0, 1),
	       "mismatch types"); /* EOVERFLOW */
}

Test(bptree_node11) {
	BpTreeNode node1;
	BpTreeItem item1;
	u64 i;

	/* Initialize leaf node */
	ASSERT(!bptree_node_init_node(&node1, 0, false), "node1_init");

	/* Insert max entries */
	for (i = 0; i < MAX_LEAF_ENTRIES; i++) {
		item1.key_len = 1;
		item1.item_type = BPTREE_ITEM_TYPE_LEAF;
		item1.vardata.kv.value_len = 1;
		item1.key = "a";
		item1.vardata.kv.value = "b";
		ASSERT(!bptree_node_insert_entry(&node1, i, &item1),
		       "insert max");
	}
	ASSERT_EQ(bptree_node_num_entries(&node1), MAX_LEAF_ENTRIES,
		  "max entries");

	/* Insert one more should fail */
	ASSERT(bptree_node_insert_entry(&node1, MAX_LEAF_ENTRIES, &item1),
	       "insert overflow entries"); /* EOVERFLOW */

	/* Wrong item type */
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	ASSERT(bptree_node_insert_entry(&node1, 0, &item1),
	       "insert wrong type"); /* Via calculate_needed <0 -> EINVAL */

	/* Invalid index */
	ASSERT(bptree_node_insert_entry(&node1, MAX_LEAF_ENTRIES + 1, &item1),
	       "insert invalid index"); /* EOVERFLOW */

	/* Delete all entries in reverse order */
	for (i = MAX_LEAF_ENTRIES; i > 0; i--) {
		ASSERT(!bptree_node_delete_entry(&node1, i - 1), "delete");
		ASSERT_EQ(bptree_node_num_entries(&node1), i - 1,
			  "num after delete");
	}
	ASSERT_EQ(bptree_node_num_entries(&node1), 0, "all deleted");
	ASSERT_EQ(bptree_node_used_bytes(&node1), 0, "used_bytes=0");

	/* Delete from empty */
	ASSERT(bptree_node_delete_entry(&node1, 0),
	       "delete empty"); /* EINVAL */
}

Test(bptree_node12) {
	BpTreeNode node1;
	BpTreeItem item1;

	/* Initialize leaf node */
	ASSERT(!bptree_node_init_node(&node1, 0, false), "node1_init");

	/* Insert some entries */
	item1.key_len = 5;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 10;
	item1.key = "key01";
	item1.vardata.kv.value = "value00000";
	ASSERT(!bptree_node_insert_entry(&node1, 0, &item1), "insert1");

	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 4;
	item1.key = "key";
	item1.vardata.kv.value = "valu";
	ASSERT(!bptree_node_insert_entry(&node1, 1, &item1), "insert2");

	/* Set to larger size */
	item1.key_len = 10;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 20;
	item1.key = "longkey123";
	item1.vardata.kv.value = "longvalue0123456789";
	ASSERT(!bptree_node_set_entry(&node1, 0, &item1), "set larger");
	ASSERT_EQ(bptree_node_key_len(&node1, 0), 10, "key_len larger");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 20, "value_len larger");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "longkey123", 10),
	       "key larger");
	ASSERT(
	    !strcmpn(bptree_node_value(&node1, 0), "longvalue0123456789", 20),
	    "value larger");

	/* Set to smaller size */
	item1.key_len = 2;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 3;
	item1.key = "sm";
	item1.vardata.kv.value = "tin";
	ASSERT(!bptree_node_set_entry(&node1, 1, &item1), "set smaller");
	ASSERT_EQ(bptree_node_key_len(&node1, 1), 2, "key_len smaller");
	ASSERT_EQ(bptree_node_value_len(&node1, 1), 3, "value_len smaller");
	ASSERT(!strcmpn(bptree_node_key(&node1, 1), "sm", 2), "key smaller");
	ASSERT(!strcmpn(bptree_node_value(&node1, 1), "tin", 3),
	       "value smaller");

	/* Set to overflow from non-overflow */
	item1.key_len = 4;
	item1.item_type = BPTREE_ITEM_TYPE_OVERFLOW;
	item1.vardata.overflow.value_len = 100;
	item1.vardata.overflow.overflow_start = 500;
	item1.vardata.overflow.overflow_end = 600;
	item1.key = "over";
	ASSERT(!bptree_node_set_entry(&node1, 0, &item1), "set to overflow");
	ASSERT_EQ(bptree_node_key_len(&node1, 0), 4, "key_len overflow");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 100, "value_len overflow");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "over", 4), "key overflow");
	ASSERT_EQ(bptree_node_is_overflow(&node1, 0), true, "is overflow");
	ASSERT_EQ(bptree_node_overflow_start(&node1, 0), 500, "start");
	ASSERT_EQ(bptree_node_overflow_end(&node1, 0), 600, "end");

	/* Set from overflow to non-overflow */
	item1.key_len = 3;
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = 5;
	item1.key = "non";
	item1.vardata.kv.value = "value";
	ASSERT(!bptree_node_set_entry(&node1, 0, &item1),
	       "set to non-overflow");
	ASSERT_EQ(bptree_node_key_len(&node1, 0), 3, "key_len non-overflow");
	ASSERT_EQ(bptree_node_value_len(&node1, 0), 5,
		  "value_len non-overflow");
	ASSERT(!strcmpn(bptree_node_key(&node1, 0), "non", 3),
	       "key non-overflow");
	ASSERT(!strcmpn(bptree_node_value(&node1, 0), "value", 5),
	       "value non-overflow");
	ASSERT_EQ(bptree_node_is_overflow(&node1, 0), false, "not overflow");

	/* Error cases for set_entry */
	ASSERT(bptree_node_set_entry(&node1, 2, &item1),
	       "set invalid index"); /* EINVAL */
	item1.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	ASSERT(bptree_node_set_entry(&node1, 0, &item1),
	       "set wrong type"); /* EINVAL via calculate_needed */
	item1.item_type = BPTREE_ITEM_TYPE_LEAF;
	item1.vardata.kv.value_len = LEAF_ARRAY_SIZE + 1;
	ASSERT(bptree_node_set_entry(&node1, 0, &item1),
	       "set size overflow"); /* EOVERFLOW */
}

Test(bptree_node13) {
	/*
	BpTreeNode node;
	BpTreeNode internal_node;
	BpTreeItem item;
	u64 i;



	ASSERT(bptree_node_init_node(NULL, 0, false) < 0, "init null");
	ASSERT_EQ(err, EINVAL, "err init null");

	ASSERT(bptree_node_set_aux(NULL, 123) < 0, "set_aux null");
	ASSERT_EQ(err, EINVAL, "err set_aux null");

	ASSERT(bptree_node_set_parent(NULL, 456) < 0, "set_parent null");
	ASSERT_EQ(err, EINVAL, "err set_parent null");

	ASSERT(bptree_node_set_copy(NULL) < 0, "set_copy null");
	ASSERT_EQ(err, EINVAL, "err set_copy null");

	ASSERT(bptree_node_unset_copy(NULL) < 0, "unset_copy null");
	ASSERT_EQ(err, EINVAL, "err unset_copy null");

	ASSERT(bptree_node_set_next_leaf(NULL, 789) < 0, "set_next_leaf null");
	ASSERT_EQ(err, EINVAL, "err set_next_leaf null");

	ASSERT(bptree_node_insert_entry(NULL, 0, &item) < 0, "insert null");
	ASSERT_EQ(err, EOVERFLOW,
		  "err insert null");

	ASSERT(bptree_node_delete_entry(NULL, 0) < 0, "delete null");
	ASSERT_EQ(err, EINVAL, "err delete null");

	ASSERT(bptree_node_set_entry(NULL, 0, &item) < 0, "set_entry null");
	ASSERT_EQ(err, EINVAL, "err set_entry null");

	ASSERT(bptree_node_move_entries(NULL, 0, &node, 0, 1) < 0,
	       "move dst null");
	ASSERT_EQ(err, EINVAL, "err move dst null");

	ASSERT(bptree_node_move_entries(&node, 0, NULL, 0, 1) < 0,
	       "move src null");
	ASSERT_EQ(err, EINVAL, "err move src null");

	ASSERT_EQ(bptree_node_parent_id(NULL), 0, "parent_id null");

	ASSERT_EQ(bptree_node_num_entries(NULL), 0, "num_entries null");

	ASSERT_EQ(bptree_node_used_bytes(NULL), 0, "used_bytes null");

	ASSERT(!bptree_node_is_copy(NULL),
	       "is_copy null");

	ASSERT(!bptree_node_is_internal(NULL),
	       "is_internal null");

	ASSERT_EQ(bptree_node_aux(NULL), 0, "aux null");

	ASSERT_EQ(bptree_node_next_leaf(NULL), 0, "next_leaf null");

	ASSERT_EQ(bptree_node_key_len(NULL, 0), 0, "key_len null");
	ASSERT_EQ(err, EINVAL, "err key_len null");

	ASSERT_EQ(bptree_node_value_len(NULL, 0), 0, "value_len null");

	ASSERT(bptree_node_key(NULL, 0) == NULL, "key null");
	ASSERT_EQ(err, EINVAL, "err key null");

	ASSERT(bptree_node_value(NULL, 0) == NULL, "value null");
	ASSERT_EQ(err, EINVAL, "err value null");

	ASSERT_EQ(bptree_node_offset(NULL, 0), 0, "offset null");
	ASSERT_EQ(err, EINVAL, "err offset null");

	ASSERT(bptree_node_is_overflow(NULL, 0) < 0, "is_overflow null");
	ASSERT_EQ(err, EINVAL, "err is_overflow null");

	ASSERT_EQ(bptree_node_overflow_start(NULL, 0), -1ULL,
		  "overflow_start null");
	ASSERT_EQ(err, EINVAL, "err overflow_start null");

	ASSERT_EQ(bptree_node_overflow_end(NULL, 0), -1ULL,
		  "overflow_end null");
	ASSERT_EQ(err, EINVAL, "err overflow_end null");

	ASSERT_EQ(bptree_node_node_id(NULL, 0), 0, "node_id null");

	ASSERT(!bptree_node_init_node(&node, 0, false), "init leaf");
	ASSERT(!bptree_node_init_node(&internal_node, 0, true),
	       "init internal");

	ASSERT_EQ(bptree_node_key_len(&node, 0), 0, "key_len invalid index");
	ASSERT_EQ(err, EINVAL, "err key_len invalid");

	ASSERT_EQ(bptree_node_value_len(&node, 0), 0,
		  "value_len invalid index");

	ASSERT(bptree_node_key(&node, 0) == NULL, "key invalid index");
	ASSERT_EQ(err, EINVAL, "err key invalid");

	ASSERT(bptree_node_value(&node, 0) == NULL, "value invalid index");
	ASSERT_EQ(err, EINVAL, "err value invalid");

	ASSERT_EQ(bptree_node_offset(&node, 0), 0, "offset invalid index");
	ASSERT_EQ(err, EINVAL, "err offset invalid");

	ASSERT(bptree_node_is_overflow(&node, 0) < 0,
	       "is_overflow invalid index");
	ASSERT_EQ(err, EINVAL, "err is_overflow invalid");

	ASSERT_EQ(bptree_node_overflow_start(&node, 0), -1ULL,
		  "overflow_start invalid");
	ASSERT_EQ(err, EINVAL, "err overflow_start invalid");

	ASSERT_EQ(bptree_node_overflow_end(&node, 0), -1ULL,
		  "overflow_end invalid");
	ASSERT_EQ(err, EINVAL, "err overflow_end invalid");

	ASSERT_EQ(bptree_node_node_id(&node, 0), 0,
		  "node_id on leaf");

	ASSERT_EQ(bptree_node_offset(&internal_node, MAX_INTERNAL_ENTRIES), 0,
		  "offset max internal");
	ASSERT_EQ(err, EINVAL, "err offset max internal");

	ASSERT_EQ(bptree_node_offset(&node, MAX_LEAF_ENTRIES), 0,
		  "offset max leaf");
	ASSERT_EQ(err, EINVAL, "err offset max leaf");

	item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	ASSERT(bptree_node_insert_entry(&node, 0, &item) < 0,
	       "insert wrong type leaf");
	ASSERT_EQ(err, EINVAL,
		  "err insert wrong");

	item.item_type = BPTREE_ITEM_TYPE_LEAF;
	ASSERT(bptree_node_insert_entry(&internal_node, 0, &item) < 0,
	       "insert wrong type internal");
	ASSERT_EQ(err, EINVAL, "err insert wrong internal");

	ASSERT(bptree_node_set_entry(&node, 0, &item) < 0,
	       "set wrong type");
	ASSERT_EQ(err, EINVAL, "err set invalid index");

	ASSERT(!bptree_node_init_node(&internal_node, 0, true),
	       "reinit internal");

	for (i = 0; i < MAX_INTERNAL_ENTRIES; i++) {
		item.key_len = 1;
		item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
		item.vardata.internal.node_id = i;
		item.key = "k";
		ASSERT(!bptree_node_insert_entry(&internal_node, i, &item),
		       "insert max internal");
	}
	ASSERT_EQ(bptree_node_num_entries(&internal_node), MAX_INTERNAL_ENTRIES,
		  "max internal entries");

	ASSERT(bptree_node_insert_entry(&internal_node, MAX_INTERNAL_ENTRIES,
					&item) < 0,
	       "insert overflow internal entries");
	ASSERT_EQ(err, EOVERFLOW, "err overflow internal entries");

	ASSERT(bptree_node_delete_entry(&node, 0) < 0, "delete empty");
	ASSERT_EQ(err, EINVAL, "err delete empty");

	ASSERT(!bptree_node_set_copy(&node), "set_copy");
	ASSERT(bptree_node_set_copy(&node) < 0, "set_copy already");
	ASSERT_EQ(err, EINVAL, "err set_copy already");

	ASSERT(!bptree_node_unset_copy(&internal_node), "unset_copy");
	ASSERT(bptree_node_unset_copy(&internal_node) < 0, "unset_copy not");
	ASSERT_EQ(err, EINVAL, "err unset_copy not");
	*/
}

Test(bptree_node14) {
	BpTreeNode dst;
	BpTreeNode src;
	BpTreeItem item;
	u64 i;

	/* Test move_entries bytes overflow */
	ASSERT(!bptree_node_init_node(&dst, 0, false), "dst init leaf");
	ASSERT(!bptree_node_init_node(&src, 0, false), "src init leaf");

	/* Fill dst with some small entries to take space */
	for (i = 0; i < 10; i++) {
		item.key_len = 1;
		item.item_type = BPTREE_ITEM_TYPE_LEAF;
		item.vardata.kv.value_len = 1;
		item.key = "a";
		item.vardata.kv.value = "b";
		ASSERT(!bptree_node_insert_entry(&dst, i, &item),
		       "dst small insert");
	}

	/* Insert large entries in src */
	item.key_len = 10;
	item.item_type = BPTREE_ITEM_TYPE_LEAF;

	item.vardata.kv.value_len = (LEAF_ARRAY_SIZE / 2) -
				    /*sizeof(BpTreeLeafEntry)*/ 8 -
				    10; /* Large but fit one */
	item.key = "longkey00";
	item.vardata.kv.value = "longvalue..."; /* Assume filled */
	ASSERT(!bptree_node_insert_entry(&src, 0, &item), "src large insert1");

	item.vardata.kv.value_len =
	    (LEAF_ARRAY_SIZE / 2) - /*sizeof(BpTreeLeafEntry)*/ 8 - 10;
	ASSERT(!bptree_node_insert_entry(&src, 1, &item), "src large insert2");

	/* Move should overflow bytes in dst */
	ASSERT(bptree_node_move_entries(&dst, 0, &src, 0, 2) < 0,
	       "move bytes overflow");
	ASSERT_EQ(err, EOVERFLOW, "err move bytes overflow");

	/* Similar for internal */
	ASSERT(!bptree_node_init_node(&dst, 0, true), "dst init internal");
	ASSERT(!bptree_node_init_node(&src, 0, true), "src init internal");

	/* Small in dst */
	for (i = 0; i < 10; i++) {
		item.key_len = 1;
		item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
		item.vardata.internal.node_id = i;
		item.key = "k";
		ASSERT(!bptree_node_insert_entry(&dst, i, &item),
		       "dst small internal");
	}

	/* Large key in src */
	item.key_len =
	    (INTERNAL_ARRAY_SIZE / 2) - /*sizeof(BpTreeInternalEntry)*/ 16;
	item.key = "longkey..."; /* Assume */
	ASSERT(!bptree_node_insert_entry(&src, 0, &item),
	       "src large internal1");

	item.key_len =
	    (INTERNAL_ARRAY_SIZE / 2) - /*sizeof(BpTreeInternalEntry)*/ 16;
	ASSERT(!bptree_node_insert_entry(&src, 1, &item),
	       "src large internal2");

	ASSERT(bptree_node_move_entries(&dst, 0, &src, 0, 2) < 0,
	       "move bytes overflow internal");
	ASSERT_EQ(err, EOVERFLOW, "err move bytes overflow internal");
}

Test(bptree_node15) {
	BpTreeNode internal_node;
	BpTreeItem item;
	BpTreeItem new_item;

	/* Test set_entry on internal node */
	ASSERT(!bptree_node_init_node(&internal_node, 0, true),
	       "init internal");

	item.key_len = 3;
	item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item.vardata.internal.node_id = 100;
	item.key = "abc";
	ASSERT(!bptree_node_insert_entry(&internal_node, 0, &item),
	       "insert internal");

	item.key_len = 4;
	item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item.vardata.internal.node_id = 200;
	item.key = "defg";
	ASSERT(!bptree_node_insert_entry(&internal_node, 1, &item),
	       "insert internal2");

	/* Set to smaller key */
	new_item.key_len = 2;
	new_item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	new_item.vardata.internal.node_id = 300;
	new_item.key = "hi";
	ASSERT(!bptree_node_set_entry(&internal_node, 0, &new_item),
	       "set smaller internal");

	ASSERT_EQ(bptree_node_key_len(&internal_node, 0), 2,
		  "key_len smaller internal");
	ASSERT(!strcmpn(bptree_node_key(&internal_node, 0), "hi", 2),
	       "key smaller internal");
	ASSERT_EQ(bptree_node_node_id(&internal_node, 0), 300,
		  "node_id smaller internal");

	/* Set to larger key */
	new_item.key_len = 5;
	new_item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	new_item.vardata.internal.node_id = 400;
	new_item.key = "jklmn";
	ASSERT(!bptree_node_set_entry(&internal_node, 1, &new_item),
	       "set larger internal");

	ASSERT_EQ(bptree_node_key_len(&internal_node, 1), 5,
		  "key_len larger internal");
	ASSERT(!strcmpn(bptree_node_key(&internal_node, 1), "jklmn", 5),
	       "key larger internal");
	ASSERT_EQ(bptree_node_node_id(&internal_node, 1), 400,
		  "node_id larger internal");

	/* Test overflow set on internal */
	new_item.key_len =
	    INTERNAL_ARRAY_SIZE - /*sizeof(BpTreeInternalEntry)*/ 16 + 1;
	new_item.key = "overflowkey...";
	ASSERT(bptree_node_set_entry(&internal_node, 0, &new_item) < 0,
	       "set overflow internal");
	ASSERT_EQ(err, EOVERFLOW, "err set overflow internal");

	/* Wrong type for set on internal */
	new_item.item_type = BPTREE_ITEM_TYPE_LEAF;
	ASSERT(bptree_node_set_entry(&internal_node, 0, &new_item) < 0,
	       "set wrong type internal");
	ASSERT_EQ(err, EINVAL, "err set wrong type internal");
}

Test(bptree_node16) {
	BpTreeNode node;
	BpTreeNode internal_node;
	BpTreeItem item;

	/* Test bptree_node_is_internal */
	ASSERT(!bptree_node_init_node(&node, 0, false), "init leaf");
	ASSERT(!bptree_node_is_internal(&node), "leaf not internal");

	ASSERT(!bptree_node_init_node(&internal_node, 0, true),
	       "init internal");
	ASSERT(bptree_node_is_internal(&internal_node), "internal is internal");

	/* Test offset on internal */
	item.key_len = 3;
	item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item.vardata.internal.node_id = 123;
	item.key = "abc";
	ASSERT(!bptree_node_insert_entry(&internal_node, 0, &item),
	       "insert offset test");

	ASSERT_EQ(bptree_node_offset(&internal_node, 0), 0,
		  "offset internal 0");

	/* Invalid offset on internal */
	ASSERT_EQ(bptree_node_offset(&internal_node, 1), 0,
		  "offset invalid index internal");
	ASSERT_EQ(err, EINVAL, "err offset invalid internal");
}

Test(bptree_node17) {
	BpTreeNode node;
	BpTreeItem item;

	/* Test !copy EACCES for insert_entry */
	ASSERT(!bptree_node_init_node(&node, 0, false), "init leaf");
	ASSERT(!bptree_node_unset_copy(&node), "unset_copy");

	item.key_len = 1;
	item.item_type = BPTREE_ITEM_TYPE_LEAF;
	item.vardata.kv.value_len = 1;
	item.key = "a";
	item.vardata.kv.value = "b";
	ASSERT(bptree_node_insert_entry(&node, 0, &item) < 0, "insert !copy");
	ASSERT_EQ(err, EACCES, "err insert !copy");

	/* Test !copy EACCES for delete_entry */
	ASSERT(!bptree_node_set_copy(&node), "set_copy");
	ASSERT(!bptree_node_insert_entry(&node, 0, &item), "insert for delete");
	ASSERT(!bptree_node_unset_copy(&node), "unset_copy");
	ASSERT(bptree_node_delete_entry(&node, 0) < 0, "delete !copy");
	ASSERT_EQ(err, EACCES, "err delete !copy");

	/* Test !copy EACCES for set_entry */
	ASSERT(!bptree_node_set_copy(&node), "set_copy");
	ASSERT(!bptree_node_insert_entry(&node, 0, &item), "insert for set");
	ASSERT(!bptree_node_unset_copy(&node), "unset_copy");
	ASSERT(bptree_node_set_entry(&node, 0, &item) < 0, "set !copy");
	ASSERT_EQ(err, EACCES, "err set !copy");

	/* Test !copy EACCES for move_entries (dst) */
	BpTreeNode src;
	ASSERT(!bptree_node_init_node(&src, 0, false), "init src");
	ASSERT(!bptree_node_insert_entry(&src, 0, &item), "insert src");
	ASSERT(bptree_node_move_entries(&node, 0, &src, 0, 1) < 0,
	       "move !copy dst");
	ASSERT_EQ(err, EACCES, "err move !copy dst");
}

Test(bptree_node18) {
	BpTreeNode internal_node;
	BpTreeItem item;
	BpTreeItem new_item;

	/* Test set_entry on internal node */
	ASSERT(!bptree_node_init_node(&internal_node, 0, true),
	       "init internal");

	item.key_len = 3;
	item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item.vardata.internal.node_id = 100;
	item.key = "abc";
	ASSERT(!bptree_node_insert_entry(&internal_node, 0, &item),
	       "insert internal1");

	item.key_len = 4;
	item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item.vardata.internal.node_id = 200;
	item.key = "defg";
	ASSERT(!bptree_node_insert_entry(&internal_node, 1, &item),
	       "insert internal2");

	/* Set to smaller key */
	new_item.key_len = 2;
	new_item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	new_item.vardata.internal.node_id = 300;
	new_item.key = "hi";
	ASSERT(!bptree_node_set_entry(&internal_node, 0, &new_item),
	       "set smaller internal");

	ASSERT_EQ(bptree_node_num_entries(&internal_node), 2,
		  "num after set internal");
	ASSERT_EQ(bptree_node_key_len(&internal_node, 0), 2,
		  "key_len smaller internal");
	ASSERT(!strcmpn(bptree_node_key(&internal_node, 0), "hi", 2),
	       "key smaller internal");
	ASSERT_EQ(bptree_node_node_id(&internal_node, 0), 300,
		  "node_id smaller internal");

	ASSERT_EQ(bptree_node_key_len(&internal_node, 1), 4,
		  "key_len unchanged");
	ASSERT(!strcmpn(bptree_node_key(&internal_node, 1), "defg", 4),
	       "key unchanged");
	ASSERT_EQ(bptree_node_node_id(&internal_node, 1), 200,
		  "node_id unchanged");

	/* Set to larger key */
	new_item.key_len = 5;
	new_item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	new_item.vardata.internal.node_id = 400;
	new_item.key = "jklmn";
	ASSERT(!bptree_node_set_entry(&internal_node, 1, &new_item),
	       "set larger internal");

	ASSERT_EQ(bptree_node_key_len(&internal_node, 1), 5,
		  "key_len larger internal");
	ASSERT(!strcmpn(bptree_node_key(&internal_node, 1), "jklmn", 5),
	       "key larger internal");
	ASSERT_EQ(bptree_node_node_id(&internal_node, 1), 400,
		  "node_id larger internal");
}

Test(bptree_node19) {
	BpTreeNode internal_node;
	BpTreeItem item;

	/* Test delete_entry on internal node */
	ASSERT(!bptree_node_init_node(&internal_node, 0, true),
	       "init internal");

	item.key_len = 2;
	item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item.vardata.internal.node_id = 10;
	item.key = "ab";
	ASSERT(!bptree_node_insert_entry(&internal_node, 0, &item),
	       "insert del1");

	item.key_len = 3;
	item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item.vardata.internal.node_id = 20;
	item.key = "cde";
	ASSERT(!bptree_node_insert_entry(&internal_node, 1, &item),
	       "insert del2");

	item.key_len = 4;
	item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
	item.vardata.internal.node_id = 30;
	item.key = "fghi";
	ASSERT(!bptree_node_insert_entry(&internal_node, 2, &item),
	       "insert del3");

	ASSERT_EQ(bptree_node_num_entries(&internal_node), 3,
		  "num before del internal");

	/* Delete middle */
	ASSERT(!bptree_node_delete_entry(&internal_node, 1),
	       "del internal middle");
	ASSERT_EQ(bptree_node_num_entries(&internal_node), 2,
		  "num after del internal");

	ASSERT_EQ(bptree_node_key_len(&internal_node, 0), 2,
		  "key_len0 after del");
	ASSERT(!strcmpn(bptree_node_key(&internal_node, 0), "ab", 2),
	       "key0 after del");
	ASSERT_EQ(bptree_node_node_id(&internal_node, 0), 10,
		  "node_id0 after del");

	ASSERT_EQ(bptree_node_key_len(&internal_node, 1), 4,
		  "key_len1 after del");
	ASSERT(!strcmpn(bptree_node_key(&internal_node, 1), "fghi", 4),
	       "key1 after del");
	ASSERT_EQ(bptree_node_node_id(&internal_node, 1), 30,
		  "node_id1 after del");

	/* Delete last */
	ASSERT(!bptree_node_delete_entry(&internal_node, 1),
	       "del internal last");
	ASSERT_EQ(bptree_node_num_entries(&internal_node), 1,
		  "num after second del");

	ASSERT_EQ(bptree_node_key_len(&internal_node, 0), 2, "key_len0 final");
	ASSERT(!strcmpn(bptree_node_key(&internal_node, 0), "ab", 2),
	       "key0 final");
	ASSERT_EQ(bptree_node_node_id(&internal_node, 0), 10, "node_id0 final");
}

Test(bptree_node20) {
	BpTreeNode node;
	BpTreeItem item;

	/* Test bptree_node_value on overflow entry */
	ASSERT(!bptree_node_init_node(&node, 0, false), "init leaf");

	item.key_len = 2;
	item.item_type = BPTREE_ITEM_TYPE_OVERFLOW;
	item.vardata.overflow.value_len = 100;
	item.vardata.overflow.overflow_start = 123;
	item.vardata.overflow.overflow_end = 456;
	item.key = "ov";

	ASSERT(!bptree_node_insert_entry(&node, 0, &item),
	       "insert overflow for value");

	ASSERT(bptree_node_value(&node, 0) == NULL, "value on overflow null");
	ASSERT_EQ(err, EINVAL, "err value on overflow");
}

Test(bptree_node21) {
	BpTreeNode node;
	BpTreeNode internal_node;

	/* Test bptree_node_is_internal calls */
	ASSERT(!bptree_node_init_node(&node, 0, false), "init leaf");
	ASSERT(!bptree_node_is_internal(&node), "leaf not internal");

	ASSERT(!bptree_node_init_node(&internal_node, 0, true),
	       "init internal");

	ASSERT(bptree_node_is_internal(&internal_node), "internal is internal");
}

Test(bptree_node22) {
	BpTreeNode node;
	BpTreeItem item;

	/* Test max fill and overflow for internal insert */
	ASSERT(!bptree_node_init_node(&node, 0, true), "init internal max");

	u64 i;
	for (i = 0; i < MAX_INTERNAL_ENTRIES; i++) {
		item.key_len = 1;
		item.item_type = BPTREE_ITEM_TYPE_INTERNAL;
		item.vardata.internal.node_id = i;
		item.key = "k";
		ASSERT(!bptree_node_insert_entry(&node, i, &item),
		       "insert max internal");
	}
	ASSERT_EQ(bptree_node_num_entries(&node), MAX_INTERNAL_ENTRIES,
		  "max internal entries");

	ASSERT(bptree_node_insert_entry(&node, MAX_INTERNAL_ENTRIES, &item) < 0,
	       "insert overflow internal entries");
	ASSERT_EQ(err, EOVERFLOW, "err overflow internal entries");

	/* Test bytes overflow insert internal */
	item.key_len =
	    INTERNAL_ARRAY_SIZE - /*sizeof(BpTreeInternalEntry)*/ 16 + 1;
	item.key = "longkeyoverflow";
	ASSERT(bptree_node_insert_entry(&node, 0, &item) < 0,
	       "insert bytes overflow internal");
	ASSERT_EQ(err, EOVERFLOW, "err bytes overflow internal");
}
