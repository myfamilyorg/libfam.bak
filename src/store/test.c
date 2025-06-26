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
#include <bptree_node.H>
#include <error.H>
#include <test.H>

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

#define MAX_LEAF_ENTRIES ((u64)(NODE_SIZE / 32))
#define MAX_INTERNAL_ENTRIES (NODE_SIZE / 16)
#define INTERNAL_ARRAY_SIZE ((28 * NODE_SIZE - 1024) / 32)
#define LEAF_ARRAY_SIZE ((u64)((30 * NODE_SIZE - 1280) / 32))

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
