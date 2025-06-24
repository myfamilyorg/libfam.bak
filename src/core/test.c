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
#include <env.H>
#include <error.H>
#include <format.H>
#include <test.H>

Test(alloc1) {
	u8 *t1, *t2, *t3, *t4, *t5, *t6, *t7;
	Alloc *a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);
	ASSERT(a, "a!=NULL");
	t1 = alloc_impl(a, CHUNK_SIZE);
	t2 = alloc_impl(a, CHUNK_SIZE);
	t3 = alloc_impl(a, CHUNK_SIZE);
	t4 = alloc_impl(a, CHUNK_SIZE);
	t5 = alloc_impl(a, CHUNK_SIZE);

	ASSERT(t1, "t1!=NULL");
	ASSERT(t2, "t2!=NULL");
	ASSERT(t3, "t3!=NULL");
	ASSERT(t4, "t4!=NULL");
	ASSERT(t5, "t5!=NULL");

	ASSERT(t1 != t2, "t1 != t2");
	ASSERT(t2 != t3, "t2 != t3");
	ASSERT(t3 != t4, "t3 != t4");
	ASSERT(t4 != t5, "t4 != t5");

	release_impl(a, t3);
	t6 = alloc_impl(a, CHUNK_SIZE);
	ASSERT_EQ(t3, t6, "last_free");

	t7 = alloc_impl(a, CHUNK_SIZE);
	ASSERT((u64)t7 > (u64)t6, "t7>t6");
	ASSERT((u64)t7 > (u64)t5, "t7>t5");

	release_impl(a, t1);
	release_impl(a, t2);
	release_impl(a, t4);
	release_impl(a, t5);
	release_impl(a, t6);
	release_impl(a, t7);

	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(alloc2) {
	Alloc *a;
	u8 *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL,
	   *t6 = NULL, *t7 = NULL;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);

	t1 = alloc_impl(a, CHUNK_SIZE / 4);

	t2 = alloc_impl(a, CHUNK_SIZE / 4);
	ASSERT_EQ((u64)t2 - (u64)t1, 4 * 1024 * 1024, "diff=4mb");

	t3 = alloc_impl(a, CHUNK_SIZE / 4);
	ASSERT_EQ((u64)t3 - (u64)t2, 4 * 1024 * 1024, "diff=4mb");

	t4 = alloc_impl(a, CHUNK_SIZE / 4);
	/* only 3 slabs fit in a chunk due to overhead */
	ASSERT((u64)t4 - (u64)t3 != 4 * 1024 * 1024, "diff!=4mb");

	t5 = alloc_impl(a, CHUNK_SIZE / 4);
	ASSERT_EQ((u64)t5 - (u64)t4, 4 * 1024 * 1024, "diff=4mb");

	t6 = alloc_impl(a, CHUNK_SIZE / 4);
	ASSERT_EQ((u64)t6 - (u64)t5, 4 * 1024 * 1024, "diff=4mb");

	t7 = alloc_impl(a, CHUNK_SIZE / 4);
	/* only 3 slabs fit in a chunk due to overhead */
	ASSERT((u64)t7 - (u64)t6 != 4 * 1024 * 1024, "diff!=4mb");

	release_impl(a, t1);
	release_impl(a, t2);
	release_impl(a, t3);
	release_impl(a, t4);
	release_impl(a, t5);
	release_impl(a, t6);
	release_impl(a, t7);

	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(resize1) {
	Alloc *a;
	u8 *t1 = NULL, *t2 = NULL, *t3 = NULL;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE * 16);

	t1 = resize_impl(a, t1, 8);
	t2 = resize_impl(a, t2, 8);
	t3 = resize_impl(a, t3, 14);

	ASSERT_EQ((u64)t2 - (u64)t1, 8, "8 byte slabs");

	release_impl(a, t1);
	resize_impl(a, t2, 0);
	resize_impl(a, t3, 0);

	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
}

Test(enomem) {
	Alloc *a;
	u8 *t1 = NULL;
	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE);
	t1 = alloc_impl(a, CHUNK_SIZE);
	ASSERT(t1, "t1 != NULL");
	err = 0;
	ASSERT(!alloc_impl(a, CHUNK_SIZE), "enomem");
	ASSERT_EQ(err, ENOMEM, "err");
	release_impl(a, t1);
	ASSERT_EQ(allocated_bytes_impl(a), 0, "alloc=0");
	alloc_destroy(a);
	ASSERT_EQ(get_allocated_bytes(), 0, "alloc=0");

	_debug_no_exit = true;
	_debug_no_write = true;
	release_impl(NULL, NULL);
	_debug_no_write = false;
	_debug_no_exit = false;
}

u64 get_memory_bytes(void);

Test(get_memory_bytes) {
	void *ptr;
	setenv("SHARED_MEMORY_BYTES", "10", true);
	_debug_no_write = true;
	ASSERT(get_memory_bytes() != 10, "invalid env value");
	_debug_no_write = false;
	unsetenv("SHARED_MEMORY_BYTES");
	err = 0;
	ASSERT_EQ(alloc_init(100, CHUNK_SIZE * 16), NULL, "invalid type");
	ASSERT_EQ(err, EINVAL, "err");
	ASSERT_EQ(alloc(CHUNK_SIZE + 1), NULL, "too big");
	ptr = alloc(8);
	ASSERT_EQ(resize(ptr, CHUNK_SIZE + 1), NULL, "too big");
	release(ptr);
}

Test(fmap) {
	const u8 *path = "/tmp/fmap.dat";
	i32 fd;
	Alloc *a;

	unlink(path);
	fd = file(path);
	fresize(fd, CHUNK_SIZE * 17);
	a = alloc_init(ALLOC_TYPE_FMAP, CHUNK_SIZE * 16, fd);
	ASSERT(a, "a!=NULL");

	alloc_destroy(a);
	close(fd);
	unlink(path);
}

Test(resize2) {
	void *t1 = alloc(CHUNK_SIZE);
	ASSERT(t1, "t1!=NULL");
	t1 = resize(t1, CHUNK_SIZE - 1);
	ASSERT(t1, "t1!=NULL");
	t1 = resize(t1, 8);
	ASSERT(t1, "t1!=NULL");
	t1 = resize(t1, 16);
	ASSERT(t1, "t1!=NULL");
	t1 = resize(t1, CHUNK_SIZE);
	ASSERT(t1, "t1!=NULL");

	_debug_no_exit = true;
	_debug_no_write = true;
	ASSERT(!resize((void *)1, 128), "invalid pointer");
	_debug_no_exit = false;
	_debug_no_write = false;

	release(t1);

	ASSERT_BYTES(0);
}

Test(cas_loop) {
	Alloc *a;
	u8 *t1 = NULL;

	a = alloc_init(ALLOC_TYPE_MAP, CHUNK_SIZE);

	_debug_cas_loop = 1;
	t1 = alloc_impl(a, 1024);
	ASSERT(t1, "t1 != NULL");
	ASSERT_EQ(_debug_cas_loop, 0, "_debug_cas_loop");
}

Test(format1) {
	i32 x = 101;
	Formatter f = {0};
	format(&f, "test1={},test2={},test3={}", 1, "abc", -100);
	ASSERT(!strcmp(format_to_string(&f), "test1=1,test2=abc,test3=-100"),
	       "strcmp");
	format_clear(&f);

	format(&f, "test1={},test2={},test3={}x", 1, "abc", -100);
	ASSERT(!strcmp(format_to_string(&f), "test1=1,test2=abc,test3=-100x"),
	       "strcmp2");

	format_clear(&f);
	format(&f, "abc");
	ASSERT(!strcmp(format_to_string(&f), "abc"), "strcmp2");

	format_clear(&f);
	format(&f, "abc={}", x);
	ASSERT(!strcmp(format_to_string(&f), "abc=101"), "strcmp3");

	format_clear(&f);
}

