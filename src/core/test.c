#include <alloc.H>
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
