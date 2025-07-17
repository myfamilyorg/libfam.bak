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
