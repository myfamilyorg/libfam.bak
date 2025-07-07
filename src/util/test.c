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
#include <atomic.H>
#include <channel.H>
#include <compress.H>
#include <crc32c.H>
#include <error.H>
#include <huffman.H>
#include <lock.H>
#include <rbtree.H>
#include <rng.H>
#include <robust.H>
#include <syscall_const.H>
#include <test.H>
#include <vec.H>

typedef struct {
	Lock lock1;
	Lock lock2;
	i32 value1;
} SharedStateData;

Test(lock) {
	Lock l1 = LOCK_INIT;
	i32 pid;
	SharedStateData *state = alloc(sizeof(SharedStateData));
	ASSERT(state != NULL, "Failed to allocate state");

	state->lock1 = LOCK_INIT;
	state->value1 = 0;

	/* Test basic lock/unlock */
	ASSERT_EQ(l1, 0, "init");
	{
		LockGuard lg1 = rlock(&l1);
		ASSERT_EQ(l1, 1, "l1=1");
	}
	ASSERT_EQ(l1, 0, "back to 0");
	{
		LockGuard lg1 = wlock(&l1);
		u32 vabc = 0x1 << 31;
		ASSERT_EQ(l1, vabc, "vabc");
	}
	ASSERT_EQ(l1, 0, "back to 0 2");

	/* Read contention */
	state->lock1 = LOCK_INIT;
	state->value1 = 0;
	if ((pid = two()) < 0) {
		ASSERT(0, "two() failed in read contention");
	}
	if (pid) {
		i32 timeout_ms = 1000;
		while (ALOAD(&state->value1) == 0 && timeout_ms > 0) {
			sleep(1);
			timeout_ms -= 1;
		}
		ASSERT(timeout_ms > 0,
		       "Timed out waiting for value1 in read contention");
		{
			LockGuard lg2 = rlock(&state->lock1);
			ASSERT_EQ(state->value1, 2,
				  "value1=2 in read contention");
		}
		if (waitid(P_PID, pid, NULL, WEXITED) < 0) {
			perror("waitid");
			ASSERT(0, "waitid failed in read contention");
		}
	} else {
		{
			LockGuard lg2 = wlock(&state->lock1);
			state->value1 = 1;
			sleep(10);
			state->value1 = 2;
		}
		exit(0);
	}

	/* Write contention */
	state->lock1 = LOCK_INIT;
	state->value1 = 0;
	if ((pid = two()) < 0) {
		ASSERT(0, "two() failed in write contention");
	}
	if (pid) {
		i32 timeout_ms = 10000;
		while (ALOAD(&state->value1) == 0 && timeout_ms > 0) {
			sleep(1);
			timeout_ms -= 1;
		}
		ASSERT(timeout_ms > 0,
		       "Timed out waiting for value1 in write contention");
		{
			LockGuard lg2 = wlock(&state->lock1);
			ASSERT_EQ(state->value1, 2,
				  "value1=2 in write contention");
		}
		if (waitid(P_PID, pid, NULL, WEXITED) < 0) {
			ASSERT(0, "waitid failed in write contention");
		}
	} else {
		{
			LockGuard lg2 = wlock(&state->lock1);
			state->value1 = 1;
			sleep(10);
			state->value1 = 2;
		}
		exit(0);
	}

	/* Write starvation prevention */
	state->lock1 = LOCK_INIT;
	state->lock2 = LOCK_INIT;
	state->value1 = 0;
	if ((pid = two()) < 0) {
		ASSERT(0, "two() failed in write starvation");
	}
	if (pid) {
		i32 timeout_ms = 1000;
		while (ALOAD(&state->lock1) == 0 && timeout_ms > 0) {
			sleep(1);
			timeout_ms -= 1;
		}
		ASSERT(timeout_ms > 0,
		       "Timed out waiting for lock1 in write starvation");
		{
			sleep(10);
			{
				LockGuard lg = wlock(&state->lock1);
				ASSERT_EQ(state->value1, 0,
					  "value1=0 in write starvation");
			}
		}
		if (waitid(P_PID, pid, NULL, WEXITED) < 0) {
			ASSERT(0, "waitid failed in write starvation parent");
		}
	} else {
		if ((pid = two()) < 0) {
			ASSERT(0, "two() failed in write starvation child");
		}
		if (pid) {
			{
				LockGuard lg = wlock(&state->lock1);
				sleep(30);
			}
			if (waitid(P_PID, pid, NULL, WEXITED) < 0) {
				ASSERT(
				    0,
				    "waitid failed in write starvation writer");
			}
			exit(0);
		} else {
			i32 timeout_ms = 1000;
			while (ALOAD(&state->lock1) == 0 && timeout_ms > 0) {
				sleep(1);
				timeout_ms -= 1;
			}
			ASSERT(timeout_ms > 0,
			       "Timed out waiting for lock1 in reader");
			{
				LockGuard lg = rlock(&state->lock1);
				state->value1 = 1;
			}
			exit(0);
		}
	}

	release(state);
}

typedef struct {
	RobustLock lock1;
	RobustLock lock2;
	i32 value1;
	i32 value2;
} RobustState;

Test(robust1) {
	RobustState *state = (RobustState *)smap(sizeof(RobustState));
	i32 cpid, i;
	state->lock1 = LOCK_INIT;
	state->value1 = 0;

	/* reap any zombie processes */
	for (i = 0; i < 10; i++) waitid(P_PID, 0, NULL, WEXITED);
	if ((cpid = two())) {
		waitid(P_PID, cpid, NULL, WEXITED);
	} else {
		if ((cpid = two())) {
			RobustGuard rg = robust_lock(&state->lock1);
			exit(0);
		} else {
			{
				sleep(100);
				{
					RobustGuard rg =
					    robust_lock(&state->lock1);
					state->value1 = 1;
				}
			}
			exit(0);
		}
	}
	while (!ALOAD(&state->value1)) yield();
	munmap(state, sizeof(RobustState));
}

Test(robust2) {
	RobustState *state = (RobustState *)smap(sizeof(RobustState));
	i32 cpid, i;
	state->lock1 = LOCK_INIT;
	state->value1 = 0;
	/* reap any zombie processes */
	for (i = 0; i < 10; i++) waitid(P_PID, 0, NULL, WEXITED);

	if ((cpid = two())) {
		sleep(10);
		{
			RobustGuard rg = robust_lock(&state->lock1);
			ASSERT_EQ(state->value1, 1, "value=1");
		}
		waitid(P_PID, cpid, NULL, WEXITED);
	} else {
		{
			RobustGuard rg = robust_lock(&state->lock1);
			sleep(100);
			state->value1 = 1;
		}
		exit(0);
	}
	munmap(state, sizeof(RobustState));
}

Test(robust3) {
	i32 lock = LOCK_INIT;
	RobustGuardImpl rg = robust_lock(&lock);
	err = SUCCESS;
	robustguard_cleanup(&rg);
	ASSERT_EQ(err, SUCCESS, "success");
	_debug_no_exit = true;
	_debug_no_write = true;
	robustguard_cleanup(&rg);
	ASSERT_EQ(err, EINVAL, "einval");
	_debug_no_write = false;
	_debug_no_exit = false;
}

typedef struct {
	i32 x;
	i32 y;
} TestMessage;

Test(channel1) {
	Channel ch1 = channel(sizeof(TestMessage));
	TestMessage msg = {0}, msg2 = {0};
	msg.x = 1;
	msg.y = 2;
	send(&ch1, &msg);
	ASSERT(!recv_now(&ch1, &msg2), "recv1");
	ASSERT_EQ(msg2.x, 1, "x=1");
	ASSERT_EQ(msg2.y, 2, "y=2");

	msg.x = 3;
	msg.y = 4;
	send(&ch1, &msg);
	msg.x = 5;
	msg.y = 6;
	send(&ch1, &msg);
	ASSERT(!recv_now(&ch1, &msg2), "recv2");
	ASSERT_EQ(msg2.x, 3, "x=3");
	ASSERT_EQ(msg2.y, 4, "y=4");
	ASSERT(!recv_now(&ch1, &msg2), "recv3");
	ASSERT_EQ(msg2.x, 5, "x=5");
	ASSERT_EQ(msg2.y, 6, "y=6");
	ASSERT(recv_now(&ch1, &msg2), "recv none");
	channel_destroy(&ch1);

	ASSERT_BYTES(0);
}

Test(channel2) {
	Channel ch1 = channel(sizeof(TestMessage));
	i32 pid;
	if ((pid = two())) {
		TestMessage msg = {0};
		recv(&ch1, &msg);
		ASSERT_EQ(msg.x, 1, "x=1");
		ASSERT_EQ(msg.y, 2, "y=2");
	} else {
		TestMessage msg = {0};
		msg.x = 1;
		msg.y = 2;
		send(&ch1, &msg);
		exit(0);
	}
	waitid(P_PID, pid, NULL, WEXITED);
	channel_destroy(&ch1);
}

Test(channel3) {
	i32 size = 100, i;
	for (i = 0; i < size; i++) {
		i32 pid;
		Channel ch1 = channel(sizeof(TestMessage));
		err = 0;
		pid = two();
		ASSERT(pid != -1, "two != -1");
		if (pid) {
			TestMessage msg = {0};
			recv(&ch1, &msg);
			ASSERT_EQ(msg.x, 1, "msg.x 1");
			ASSERT_EQ(msg.y, 2, "msg.y 2");
			recv(&ch1, &msg);
			ASSERT_EQ(msg.x, 3, "msg.x 3");
			ASSERT_EQ(msg.y, 4, "msg.y 4");
			recv(&ch1, &msg);
			ASSERT_EQ(msg.x, 5, "msg.x 5");
			ASSERT_EQ(msg.y, 6, "msg.y 6");
			ASSERT_EQ(recv_now(&ch1, &msg), -1, "recv_now");
			ASSERT(!waitid(0, pid, NULL, 4), "waitpid");
		} else {
			TestMessage msg = {0};
			msg.x = 1;
			msg.y = 2;
			ASSERT(!send(&ch1, &msg), "send1");
			msg.x = 3;
			msg.y = 4;
			ASSERT(!send(&ch1, &msg), "send2");
			msg.x = 5;
			msg.y = 6;
			ASSERT(!send(&ch1, &msg), "send3");
			exit(0);
		}
		waitid(P_PID, pid, NULL, WEXITED);
		channel_destroy(&ch1);
		ASSERT_BYTES(0);
	}
}

Test(channel_notify) {
	Channel ch1 = channel(sizeof(TestMessage));
	Channel ch2 = channel(sizeof(TestMessage));

	i32 pid = two();
	ASSERT(pid >= 0, "pid>=0");
	if (pid) {
		TestMessage msg = {0}, msg2 = {0};
		msg.x = 100;
		sleep(10);
		send(&ch2, &msg);

		recv(&ch1, &msg2);
		ASSERT_EQ(msg2.x, 1, "msg.x");
		ASSERT_EQ(msg2.y, 2, "msg.y");
		ASSERT_EQ(recv_now(&ch1, &msg), -1, "recv_now");

	} else {
		TestMessage msg = {0};
		recv(&ch2, &msg);

		ASSERT_EQ(msg.x, 100, "x=100");
		msg.x = 1;
		msg.y = 2;
		send(&ch1, &msg);
		exit(0);
	}
	waitid(P_PID, pid, NULL, WEXITED);

	channel_destroy(&ch1);
	channel_destroy(&ch2);
}

Test(channel_cycle) {
	i32 pid, i;
	Channel ch1 = channel2(sizeof(TestMessage), 8);
	TestMessage msg;
	msg.x = 1;
	msg.y = 2;
	for (i = 0; i < 8; i++) send(&ch1, &msg);
	recv(&ch1, &msg);
	recv(&ch1, &msg);
	msg.x = 1;
	msg.y = 2;
	send(&ch1, &msg);

	if ((pid = two())) {
		msg.x = 0;
		recv(&ch1, &msg);
		ASSERT_EQ(msg.x, 1, "1");
	} else {
		sleep(10);
		send(&ch1, &msg);
		exit(0);
	}
	waitid(P_PID, pid, NULL, WEXITED);
}

Test(channel_err) {
	TestMessage msg;
	i32 i;
	Channel ch1, ch2, ch3;

	ASSERT_BYTES(0);
	err = 0;
	ch1 = channel(0);
	ASSERT_EQ(err, EINVAL, "einval");
	ASSERT(!channel_ok(&ch1), "ok");
	err = 0;
	ch2 = channel2(8, 0);
	ASSERT_EQ(err, EINVAL, "einval2");
	ASSERT(!channel_ok(&ch1), "ok2");

	ch3 = channel2(sizeof(TestMessage), 8);
	for (i = 0; i < 8; i++) {
		msg.x = 1;
		msg.y = 2;
		ASSERT(!send(&ch3, &msg), "sendmsg");
	}
	msg.x = 1;
	msg.y = 2;
	ASSERT(send(&ch3, &msg), "senderr");

	channel_destroy(&ch1);
	channel_destroy(&ch2);
	channel_destroy(&ch3);

	ASSERT_BYTES(0);
}

typedef struct {
	RbTreeNode _reserved;
	u64 value;
} TestRbTreeNode;

i32 test_rbsearch(RbTreeNode *cur, const RbTreeNode *value,
		  RbTreeNodePair *retval) {
	while (cur) {
		u64 v1 = ((TestRbTreeNode *)cur)->value;
		u64 v2 = ((TestRbTreeNode *)value)->value;
		if (v1 == v2) {
			retval->self = cur;
			break;
		} else if (v1 < v2) {
			retval->parent = cur;
			retval->is_right = 1;
			cur = cur->right;
		} else {
			retval->parent = cur;
			retval->is_right = 0;
			cur = cur->left;
		}
		retval->self = cur;
	}
	return 0;
}

#define PARENT(node) ((RbTreeNode *)((u64)node->parent_color & ~0x1))
#define RIGHT(node) node->right
#define LEFT(node) node->left
#define ROOT(tree) (tree->root)
#define IS_RED(node) (node && ((u64)node->parent_color & 0x1))
#define IS_BLACK(node) !IS_RED(node)
#define ROOT(tree) (tree->root)

static bool check_root_black(RbTree *tree) {
	if (!ROOT(tree)) return true;
	return IS_BLACK(ROOT(tree));
}

bool check_no_consecutive_red(RbTreeNode *node) {
	if (!node) return true;

	if (IS_RED(node)) {
		if (RIGHT(node) && IS_RED(RIGHT(node))) return false;
		if (LEFT(node) && IS_RED(LEFT(node))) return false;
	}

	return check_no_consecutive_red(LEFT(node)) &&
	       check_no_consecutive_red(RIGHT(node));
}

i32 check_black_height(RbTreeNode *node) {
	i32 left_height, right_height;
	if (!node) return 1;
	left_height = check_black_height(LEFT(node));
	right_height = check_black_height(RIGHT(node));

	if (left_height == -1 || right_height == -1) return -1;

	if (left_height != right_height) return -1;

	return left_height + (IS_BLACK(node) ? 1 : 0);
}

static void validate_rbtree(RbTree *tree) {
	ASSERT(check_root_black(tree), "Root must be black");
	ASSERT(check_no_consecutive_red(ROOT(tree)),
	       "No consecutive red nodes");
	ASSERT(check_black_height(ROOT(tree)) != -1,
	       "Inconsistent black height");
}

Test(rbtree1) {
	RbTree tree = RBTREE_INIT;
	TestRbTreeNode v1 = {{0}, 1};
	TestRbTreeNode v2 = {{0}, 2};
	TestRbTreeNode v3 = {{0}, 3};
	TestRbTreeNode v4 = {{0}, 0};
	TestRbTreeNode vx = {{0}, 3};
	TestRbTreeNode vy = {{0}, 0};
	TestRbTreeNode *out, *out2;
	RbTreeNodePair retval = {0};

	rbtree_put(&tree, (RbTreeNode *)&v1, test_rbsearch);
	validate_rbtree(&tree);
	rbtree_put(&tree, (RbTreeNode *)&v2, test_rbsearch);
	validate_rbtree(&tree);

	test_rbsearch(tree.root, (RbTreeNode *)&v1, &retval);
	ASSERT_EQ(((TestRbTreeNode *)retval.self)->value, 1, "value=1");

	test_rbsearch(tree.root, (RbTreeNode *)&v2, &retval);
	ASSERT_EQ(((TestRbTreeNode *)retval.self)->value, 2, "value=2");
	test_rbsearch(tree.root, (RbTreeNode *)&v3, &retval);
	ASSERT_EQ(retval.self, NULL, "self=NULL");

	rbtree_remove(&tree, (RbTreeNode *)&v2, test_rbsearch);
	validate_rbtree(&tree);
	test_rbsearch(tree.root, (RbTreeNode *)&v2, &retval);
	ASSERT_EQ(retval.self, NULL, "retval=NULL2");

	rbtree_put(&tree, (RbTreeNode *)&v3, test_rbsearch);
	validate_rbtree(&tree);
	rbtree_put(&tree, (RbTreeNode *)&v4, test_rbsearch);
	validate_rbtree(&tree);

	out = (TestRbTreeNode *)rbtree_put(&tree, (RbTreeNode *)&vx,
					   test_rbsearch);
	validate_rbtree(&tree);
	ASSERT_EQ(out, &v3, "out=v3");

	out2 = (TestRbTreeNode *)rbtree_put(&tree, (RbTreeNode *)&vy,
					    test_rbsearch);
	validate_rbtree(&tree);
	ASSERT_EQ(out2, &v4, "out2=v4");
}

#define SIZE 400

Test(rbtree2) {
	Rng rng;
	u64 size, i;

	ASSERT(!rng_init(&rng), "rng_init");

	for (size = 1; size < SIZE; size++) {
		RbTree tree = RBTREE_INIT;
		TestRbTreeNode values[SIZE];
		for (i = 0; i < size; i++) {
			rng_gen(&rng, &values[i].value, sizeof(u64));
			rbtree_put(&tree, (RbTreeNode *)&values[i],
				   test_rbsearch);
			validate_rbtree(&tree);
		}

		for (i = 0; i < size; i++) {
			RbTreeNodePair retval = {0};
			TestRbTreeNode v = {{0}, 0};
			v.value = values[i].value;

			test_rbsearch(tree.root, (RbTreeNode *)&v, &retval);
			ASSERT(retval.self != NULL, "retval=NULL");
			ASSERT_EQ(((TestRbTreeNode *)retval.self)->value,
				  values[i].value, "value=values[i].value");
		}

		for (i = 0; i < size; i++) {
			TestRbTreeNode v = {{0}, 0};
			v.value = values[i].value;
			rbtree_remove(&tree, (RbTreeNode *)&v, test_rbsearch);
			validate_rbtree(&tree);
		}

		ASSERT_EQ(tree.root, NULL, "root=NULL");
		validate_rbtree(&tree);
	}
}

#define STRESS_SIZE 1000
#define OPERATIONS 2000

Test(rbtree3) {
	Rng rng;
	RbTree tree = RBTREE_INIT;
	TestRbTreeNode values[STRESS_SIZE] = {0};
	bool exists[STRESS_SIZE] = {0};
	u64 i, op;

	ASSERT(!rng_init(&rng), "rng_init");

	for (i = 0; i < STRESS_SIZE; i++) {
		rng_gen(&rng, &values[i].value, sizeof(u64));
	}

	for (op = 0; op < OPERATIONS; op++) {
		i32 i;
		u64 idx;
		bool do_insert = false;
		rng_gen(&rng, &idx, sizeof(u64));
		idx %= STRESS_SIZE;
		rng_gen(&rng, &i, sizeof(i32));
		do_insert = (i % 2) == 0;

		if (do_insert) {
			TestRbTreeNode *old = (TestRbTreeNode *)rbtree_put(
			    &tree, (RbTreeNode *)&values[idx], test_rbsearch);
			if (old) {
				ASSERT_EQ(old->value, values[idx].value,
					  "Duplicate value mismatch");
			}
			exists[idx] = true;
			validate_rbtree(&tree);
		} else {
			TestRbTreeNode v = {0};
			v.value = values[idx].value;
			rbtree_remove(&tree, (RbTreeNode *)&v, test_rbsearch);
			exists[idx] = false;
			validate_rbtree(&tree);
		}

		for (i = 0; i < STRESS_SIZE; i++) {
			RbTreeNodePair retval = {0};
			TestRbTreeNode v = {0};
			v.value = values[i].value;
			test_rbsearch(tree.root, (RbTreeNode *)&v, &retval);
			if (exists[i]) {
				ASSERT(retval.self != NULL,
				       "Expected value not found");
				ASSERT_EQ(
				    ((TestRbTreeNode *)retval.self)->value,
				    values[i].value, "Wrong value");
			} else {
				ASSERT_EQ(retval.self, NULL,
					  "Unexpected value found");
			}
		}
	}

	for (i = 0; i < STRESS_SIZE; i++) {
		if (exists[i]) {
			TestRbTreeNode v = {0};
			v.value = values[i].value;
			rbtree_remove(&tree, (RbTreeNode *)&v, test_rbsearch);
			validate_rbtree(&tree);
		}
	}

	ASSERT_EQ(tree.root, NULL, "Tree not empty after cleanup");
	validate_rbtree(&tree);
}

Test(vec1) {
	u8 buf[100] = {0};
	Vec *v = vec_new(100);
	ASSERT(v, "v!=NULL");
	ASSERT_EQ(vec_capacity(v), 100, "cap=100");
	ASSERT_EQ(vec_elements(v), 0, "elem=0");
	ASSERT(!vec_extend(v, "abcdefg", 7), "vec_extend");
	ASSERT_EQ(vec_capacity(v), 100, "cap=100");
	ASSERT_EQ(vec_elements(v), 7, "elem=7");
	ASSERT(!strcmpn("abcdefg", vec_data(v), 7), "data value");
	ASSERT(vec_extend(v, buf, 100), "overflow");
	vec_truncate(v, 4);
	ASSERT_EQ(vec_elements(v), 4, "elem=4");
	ASSERT_EQ(vec_capacity(v), 100, "capacity=100");
	v = vec_resize(v, 3);
	ASSERT_EQ(vec_elements(v), 3, "elem=3");
	ASSERT_EQ(vec_capacity(v), 3, "capacity=3");
	ASSERT(vec_truncate(v, 5), "truncate error");
	ASSERT(!vec_set_elements(v, 2), "set elements");
	ASSERT_EQ(vec_elements(v), 2, "elem=2");
	ASSERT(vec_set_elements(v, 10), "set elements err");
	vec_release(v);
	ASSERT_BYTES(0);
}

Test(huffman1) {
	int i;
	HuffmanLookup lookup = {0};
	const u8 *in1 = "abcdbxxx";
	const u8 *in2 = "alkjsdfasghalshgaslfdjadslf;l54(*4";
	int count = 0;
	huffman_gen(&lookup, in1, strlen(in1));
	for (i = 0; i < 256; i++) {
		if (lookup.lengths[i]) count++;
	}
	ASSERT_EQ(count, 5, "count=5");
	ASSERT(!lookup.lengths['e'], "lookup e == 0");
	ASSERT(lookup.lengths['b'] < lookup.lengths['a'], "lookup b < a");
	ASSERT(lookup.lengths['x'] < lookup.lengths['b'], "lookup x < b");

	memset(&lookup, 0, sizeof(HuffmanLookup));
	huffman_gen(&lookup, in2, strlen(in2));
	count = 0;
	for (i = 0; i < 256; i++)
		if (lookup.lengths[i]) ++count;

	ASSERT_EQ(count, 14, "count=14");
}

Test(huffman_encode1) {
	HuffmanLookup lookup = {0};
	i32 out_len;
	u8 buf[1024];
	u8 verify[1024];
	const u8 *in = "alkjsdfasghalshgaslfdjadslf;l54(*4";
	huffman_gen(&lookup, in, strlen(in));
	out_len = huffman_encode(&lookup, in, strlen(in), buf, sizeof(buf));
	ASSERT(out_len > 0, "out_len>0");
	out_len = huffman_decode(buf, out_len, verify, sizeof(verify));
	ASSERT_EQ(strlen(in), (u64)out_len, "len match");
	verify[out_len] = 0;
	ASSERT(!strcmp(in, verify), "in=verify");
}

Test(huffman_encode2) {
	int i;
	for (i = 0; i < 10; i++) {
		HuffmanLookup lookup = {0};
		const u8 *path = "./resources/test_long.txt";
		i32 fd = file(path);
		u64 len = fsize(fd);
		i32 out_len;
		void *ptr;
		u8 buf[U16_MAX * 2];
		u8 verify[U16_MAX];

		ASSERT(fd > 0, "fd>0");
		ptr = fmap(fd, len, 0);
		ASSERT(ptr, "ptr");

		huffman_gen(&lookup, ptr, len);
		out_len = huffman_encode(&lookup, ptr, len, buf, sizeof(buf));
		println("compress={},data={}", out_len, len);
		out_len = huffman_decode(buf, out_len, verify, sizeof(verify));
		ASSERT_EQ((u64)out_len, len, "len match");
		ASSERT(!strcmpn(ptr, verify, len), "data equal");

		munmap(ptr, len);
		close(fd);
	}
}

/*
Test(compress1) {
	u8 out[131070];
	i32 res, i = 0, j = 0;
	u32 crc = 0;
	u8 *large_input = NULL;

	ASSERT(!large_input && !i && !j && !crc, "init");

	res = lzx_compress_block("test12test12", 12, out, sizeof(out));
	ASSERT_EQ(res, 10, "res=10");
	ASSERT(!memcmp(out, (u8[]){'t', 'e', 's', 't', '1', '2', 0xFD, 6, 0, 0},
		       10),
	       "test12test12");

	res = lzx_compress_block("helloxvhelloxv", 14, out, sizeof(out));
	ASSERT_EQ(res, 11, "res=11");

	ASSERT(!memcmp(out,
		       (u8[]){'h', 'e', 'l', 'l', 'o', 'x', 'v', 0xFD, 7, 0, 0},
		       11),
	       "helloxvhelloxv");



	res = lzx_compress_block("hellohello2", 11, out, sizeof(out));
	ASSERT_EQ(res, 10, "res=10");
	ASSERT(!memcmp(out, (u8[]){'h', 'e', 'l', 'l', 'o', 0xFD, 5, 0, 0, '2'},
		       10),
	       "hellohello2");

	res = lzx_compress_block("1hellohello2", 12, out, sizeof(out));
	ASSERT_EQ(res, 11, "res=11");
	ASSERT(!memcmp(out,
		       (u8[]){'1', 'h', 'e', 'l', 'l', 'o', 0xFD, 5, 1, 0, '2'},
		       11),
	       "1hellohello2");

	res = lzx_compress_block((u8 *)"test\xFDtest", 9, out, sizeof(out));
	ASSERT_EQ(res, 10, "res=10");
	ASSERT(
	    !memcmp(out, (u8[]){'t', 'e', 's', 't', 0xFD, 0x00, 0xFD, 4, 0, 0},
		    10),
	    "test\xFDtest");

	res = lzx_compress_block((u8 *)"abc", 3, out, sizeof(out));
	ASSERT_EQ(res, 3, "res=3");
	ASSERT(!memcmp(out, (u8[]){'a', 'b', 'c'}, 3), "abc");

	res = lzx_compress_block((u8 *)"testtest", 8, out, 4);
	ASSERT_EQ(res, -1, "res=-1");
	ASSERT_EQ(err, ENOBUFS, "err=ENOBUFS");

	res = lzx_compress_block(NULL, 8, out, sizeof(out));
	ASSERT_EQ(res, -1, "res=-1");
	ASSERT_EQ(err, EINVAL, "err=EINVAL");

	res = lzx_compress_block((u8 *)"test", 0, out, sizeof(out));
	ASSERT_EQ(res, -1, "res=-1");
	ASSERT_EQ(err, EINVAL, "err=EINVAL");

	large_input = alloc(258);
	memcpy(large_input, "test", 4);
	memset(large_input + 4, 'x', 250);
	memcpy(large_input + 254, "test", 4);
	res = lzx_compress_block(large_input, 258, out, sizeof(out));
	ASSERT_EQ(res, 258, "res=258");
	ASSERT(!memcmp(out, (u8[]){'t', 'e', 's', 't'}, 4),
	       "large match first 4 bytes");
	ASSERT(!memcmp(out + 4, (u8[]){'x', 'x', 'x', 'x'}, 4),
	       "large match middle 4 bytes");
	ASSERT(!memcmp(out + 254, (u8[]){0xFD, 0x4, 0x00, 0x00}, 4),
	       "large match last 4 bytes");
	release(large_input);

	large_input = alloc(65535);
	memset(large_input, 0xFD, 65535);
	res = lzx_compress_block(large_input, 65535, out, sizeof(out));
	ASSERT_EQ(res, 131070, "res=131070 (all 0xFD, escape-only)");
	for (i = 0; i < 131070; i += 2) {
		ASSERT_EQ(out[i], 0xFD, "all 0xFD: FD at even indices");
		ASSERT_EQ(out[i + 1], 0x00, "all 0xFD: 0x00 at odd indices");
	}
	release(large_input);

	large_input = alloc(65535);
	for (i = 0; i < 65535; i++) {
		large_input[i] = (i % 10 == 0) ? 0xFD : 'a';
	}
	res = lzx_compress_block(large_input, 65535, out, sizeof(out));
	ASSERT_EQ(res, 72089, "res=72089 (10% 0xFD, escape-only)");
	for (i = 0, j = 0; i < 65535; i++) {
		if (i % 10 == 0) {
			ASSERT_EQ(out[j], 0xFD, "mixed 0xFD: FD");
			ASSERT_EQ(out[j + 1], 0x00, "mixed 0xFD: 0x00");
			j += 2;
		} else {
			ASSERT_EQ(out[j], 'a', "mixed 0xFD: 'a'");
			j++;
		}
	}
	release(large_input);

	large_input = alloc(65535);
	{
		u32 x = 0;
		for (i = 0; i < 65535; i++) {
			x = (1664525 * x + 1013904223);
			large_input[i] =
			    (u8)(x >> 24) == 0xFD ? 0xFE : (u8)(x >> 24);
		}
	}
	res = lzx_compress_block(large_input, 65535, out, sizeof(out));
	ASSERT_EQ(res, 65535, "res=65535 (incompressible, no matches)");
	ASSERT(!memcmp(out, large_input, 65535),
	       "incompressible: output matches input");
	release(large_input);

	large_input = alloc(65535);
	{
		u64 x = 0;
		memcpy(large_input, "test", 4);
		for (i = 4; i < 65531; i++) {
			x = (1664525 * x + 1013904223);
			large_input[i] =
			    (u8)(x >> 24) == 0xFD ? 0xFE : (u8)(x >> 24);
		}
	}
	memcpy(large_input + 65531, "test\0", 4);
	large_input[65534] = 0xFD;
	res = lzx_compress_block(large_input, 65535, out, sizeof(out));
	ASSERT_EQ(res, 65536, "res=65536 (match after 64KB, escape-only)");
	ASSERT(!memcmp(out, large_input, 65534),
	       "near-overflow: first 65534 bytes");
	ASSERT_EQ(out[65534], 0xFD, "near-overflow: FD");
	ASSERT_EQ(out[65535], 0x00, "near-overflow: 0x00");
	release(large_input);

	large_input = alloc(32768);
	memset(large_input, 0xFD, 32768);
	res = lzx_compress_block(large_input, 32768, out, sizeof(out));
	ASSERT_EQ(res, 65536, "res=65536 (exactly 64KB, escape-only)");
	for (i = 0; i < 65536; i += 2) {
		ASSERT_EQ(out[i], 0xFD, "64KB: FD at even indices");
		ASSERT_EQ(out[i + 1], 0x00, "64KB: 0x00 at odd indices");
	}
	release(large_input);

	large_input = alloc(65535);
	memset(large_input, 0xFD, 65535);
	res = lzx_compress_block(large_input, 65535, out, 131069);
	ASSERT_EQ(res, -1, "res=-1 (insufficient out_capacity)");
	ASSERT_EQ(err, ENOBUFS, "err=ENOBUFS (insufficient out_capacity)");
	release(large_input);

	large_input = alloc(65535);
	memcpy(large_input, "test", 4);
	memset(large_input + 4, 'x', 65531);
	res = lzx_compress_block(large_input, 65535, out, sizeof(out));
	ASSERT(res > 0, "res>0 (large input for CRC32c)");
	crc = crc32c(out, res);
	ASSERT(crc != 0, "CRC32c non-zero");
	out[0] ^= 0x01;
	ASSERT(crc32c(out, res) != crc, "CRC32c detects corruption");
	release(large_input);
}

Test(compress_file1) {
	int i, count = 1;
	for (i = 0; i < count; i++) {
		const u8 *path = "./resources/test_long.txt";
		i32 fd = file(path);
		u64 len = fsize(fd);
		i32 res = 0;
		void *ptr;
		u8 out[120000];

		ASSERT(fd > 0, "fd>0");
		ptr = fmap(fd, len, 0);
		ASSERT(ptr, "ptr");

		res = compress_block(ptr, len, out, sizeof(out));
		ASSERT(res, "res!=0");

		munmap(ptr, len);
		close(fd);
	}
}

Test(compress_rle) {
	i32 res;
	u8 out[27];
	u8 verify[512];
	const char *x =
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx"
	    "xxxxxxxxxxxx";

	res = lzx_compress_block(x, strlen(x), out, sizeof(out));
	ASSERT_EQ(res, 26, "res=26");
	res = lzx_decompress_block(out, res, verify, sizeof(verify));
	ASSERT_EQ((u64)res, strlen(x), "res=strlen(x)");
	ASSERT(!strcmpn(x, verify, res), "in=out");
}
*/
