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

#include <libfam/lmdb.H>
#include <libfam/midl.H>
#include <libfam/test.H>

Test(lmdb1) {
	const u8 *path = "/tmp/lmdb1";
	const u8 *path_lock = "/tmp/lmdb1-lock";
	MDB_env *env;
	MDB_txn *txn;
	MDB_dbi dbi;

	unlink(path);
	unlink(path_lock);

	ASSERT_BYTES(0);

	ASSERT(!mdb_env_create(&env), "mdb_env_create success");

	ASSERT(!mdb_env_open(env, path, MDB_NOSUBDIR | MDB_WRITEMAP | MDB_NOTLS,
			     0664),
	       "mdb_env_open success");

	ASSERT(!mdb_txn_begin(env, NULL, 0, &txn), "mdb_txn_begin success");

	ASSERT(!mdb_dbi_open(txn, NULL, 0, &dbi), "mdb_dbi_open success");

	MDB_val key, val_put, val_get;
	char key_str[] = "test_key";
	char val_str[] = "test_value";
	key.mv_size = sizeof(key_str);
	key.mv_data = key_str;
	val_put.mv_size = sizeof(val_str);
	val_put.mv_data = val_str;

	ASSERT(!mdb_put(txn, dbi, &key, &val_put, 0), "mdb_put success");

	ASSERT(!mdb_txn_commit(txn), "mdb_txn_commit success");

	ASSERT(!mdb_txn_begin(env, NULL, MDB_RDONLY, &txn),
	       "mdb_txn_begin (read) success");

	ASSERT(!mdb_get(txn, dbi, &key, &val_get), "mdb_get success");

	ASSERT(
	    val_get.mv_size == val_put.mv_size &&
		memcmp(val_get.mv_data, val_put.mv_data, val_put.mv_size) == 0,
	    "value match");

	mdb_txn_abort(txn);

	mdb_dbi_close(env, dbi);

	mdb_env_close(env);

	unlink(path);
	unlink(path_lock);

	ASSERT_BYTES(0);
}

/* Basic allocation and free test */
Test(midl_basic) {
	MDB_IDL ids;

	ids = mdb_midl_alloc(2);
	ASSERT(ids != NULL, "mdb_midl_alloc success");
	ASSERT(ids[-1] == 2, "initial capacity");
	ASSERT(ids[0] == 0, "initial count 0");

	mdb_midl_free(ids);
	ASSERT_BYTES(0);
}

/* Append and grow test */
Test(midl_append_and_grow) {
	MDB_IDL ids;
	i32 rc;

	ids = mdb_midl_alloc(2);
	ASSERT(ids != NULL, "alloc success");
	ASSERT(ids[-1] == 2, "initial capacity");

	rc = mdb_midl_append(&ids, 100);
	ASSERT(rc == 0, "first append");
	ASSERT(ids[0] == 1, "count after first");
	ASSERT(ids[1] == 100, "value after first");

	rc = mdb_midl_append(&ids, 200);
	ASSERT(rc == 0, "second append");
	ASSERT(ids[0] == 2, "count after second");
	ASSERT(ids[2] == 200, "value after second");
	ASSERT(ids[-1] == 2, "capacity unchanged");

	/* Third append should trigger grow */
	rc = mdb_midl_append(&ids, 300);
	ASSERT(rc == 0, "third append (grow)");
	ASSERT(ids[0] == 3, "count after grow");
	ASSERT(ids[3] == 300, "value after grow");
	ASSERT(ids[-1] >= 2 + MDB_IDL_UM_MAX,
	       "capacity grown"); /* grow adds MDB_IDL_UM_MAX */

	mdb_midl_free(ids);
	ASSERT_BYTES(0);
}

/* Sort and search test */
Test(midl_sort_and_search) {
	MDB_IDL ids;
	u32 pos;

	ids = mdb_midl_alloc(5);
	ASSERT(ids != NULL, "alloc success");

	/* Append unsorted values */
	mdb_midl_append(&ids, 30);
	mdb_midl_append(&ids, 10);
	mdb_midl_append(&ids, 50);
	mdb_midl_append(&ids, 40);
	mdb_midl_append(&ids, 20);

	ASSERT(ids[0] == 5, "count before sort");

	/* Sort: note from code, it sorts in descending order? Wait, check the
	 * sort logic. */
	/* In insertion sort: if (ids[i] >= a) break; ids[i+1] = ids[i]; ...
	 * ids[i+1]=a */
	/* This is inserting such that larger values are first? No: */
	/* for i = j-1 down to 1, if ids[i] >= a break, else shift up. So if
	 * ids[i] < a, it doesn't shift? No: */
	/* Wait: the condition is if (ids[i] >= a) break; so it's shifting if
	 * ids[i] < a? No: */
	/* Standard insertion sort for ascending? Wait: if ids[i] >= a, break,
	 * meaning stop shifting if current is >= a, so a is inserted after
	 * larger ones? No. */
	/* Typical ascending: while i>=0 and arr[i] > key, arr[i+1]=arr[i], i--
	 */
	/* Here: if (ids[i] >= a) break; else ids[i+1]=ids[i] */
	/* So break if ids[i] >= a, meaning shift if ids[i] < a, which is for
	 * descending order: larger elements are shifted right if smaller than
	 * a? Wait. */
	/* Example: suppose sorted [5,4,3], insert 6: a=6, i starts at 2 (3), 3
	 * <6? yes, shift 3 to pos3, i=1 (4), 4<6 yes, shift to pos2, i=0 (5),
	 * 5<6 yes, shift to pos1, i=-1, put 6 at pos0+1=1? Wait, code:
	 * for(i=j-1;i>=1;i--), so stops at 1, not 0? */
	/* The loop is for(i=j-1;i>=1;i--), so doesn't check i=0. This seems
	 * odd. */
	/* Original LMDB midl_sort is quicksort with insertion, and it's sorting
	 * page numbers in descending order, yes, because free pages are
	 * allocated from high to low. */
	/* In the quicksort part: while (ids[i] > a), a is pivot, do i++ while
	 * ids[i] > a, j-- while ids[j] < a, so it's partitioning with larger on
	 * left, smaller on right? Wait. */
	/* Standard quicksort for ascending is i++ while arr[i] < pivot, j--
	 * while arr[j] > pivot. */
	/* Here: i++ while ids[i] > a, j-- while ids[j] < a, so it's reversed:
	 * smaller on left, larger on right? No: */
	/* After swap, and pivot placement, but the choice is median, and swaps
	 * to make pivot in place. */
	/* But in LMDB, the ID lists are sorted descending, as free pages are
	 * popped from the end. */
	/* Confirm from code: in insertion: if (ids[i] >= a) break; so if ids[i]
	 * < a, shift ids[i] to i+1, so moving smaller to right, inserting a
	 * left of smaller, so larger left, descending. Yes. */

	mdb_midl_sort(ids);
	ASSERT(ids[1] == 50, "sorted desc [1]");
	ASSERT(ids[2] == 40, "sorted desc [2]");
	ASSERT(ids[3] == 30, "sorted desc [3]");
	ASSERT(ids[4] == 20, "sorted desc [4]");
	ASSERT(ids[5] == 10, "sorted desc [5]");

	/* Search: binary search for position, returns cursor where val >0 means
	 * insert after, etc. */
	/* CMP(x,y) = x<y -1, x>y 1, ==0 */
	/* val = CMP(ids[cursor], id), so if ids[cursor] < id, val=-1, then
	 * n=pivot (left half) */
	/* If val >0 ids[cursor] > id, go right. So it's searching assuming
	 * descending sorted, finding first <= id or insert point. */
	/* If found returns cursor, else if val>0 (last cmp ids[cursor]>id),
	 * ++cursor (insert point) */
	pos = mdb_midl_search(ids, 30);
	ASSERT(pos == 3, "search for existing 30");

	pos = mdb_midl_search(ids, 25);
	ASSERT(pos == 4, "search for 25 (insert point)");

	pos = mdb_midl_search(ids, 5);
	ASSERT(pos == 6, "search below smallest (beyond end)");

	pos = mdb_midl_search(ids, 60);
	ASSERT(pos == 1, "search above largest (at start)");

	mdb_midl_free(ids);
	ASSERT_BYTES(0);
}

/* Append range and need test */
Test(midl_append_range_and_need) {
	MDB_IDL ids;
	i32 rc;

	ids = mdb_midl_alloc(3);
	ASSERT(ids != NULL, "alloc success");

	rc = mdb_midl_append_range(&ids, 100, 2);
	ASSERT(rc == 0, "append range 2");
	ASSERT(ids[0] == 2, "count after range");
	ASSERT(ids[1] == 101, "range value 1");
	ASSERT(ids[2] == 100, "range value 2");

	/* Need more capacity */
	rc = mdb_midl_need(&ids, 5);
	ASSERT(rc == 0, "mdb_midl_need success");
	ASSERT(ids[0] == 2, "count unchanged after need");
	ASSERT(ids[-1] >= 2 + 5, "capacity increased");

	rc = mdb_midl_append_range(&ids, 200, 4);
	ASSERT(rc == 0, "append range 4 after need");
	ASSERT(ids[0] == 6, "count after second range");
	ASSERT(ids[3] == 203, "range value 3");
	ASSERT(ids[4] == 202, "range value 4");
	ASSERT(ids[5] == 201, "range value 5");
	ASSERT(ids[6] == 200, "range value 6");

	mdb_midl_free(ids);
	ASSERT_BYTES(0);
}

/* Additional test: shrink */
Test(midl_shrink) {
	MDB_IDL ids;

	/* Alloc large */
	ids = mdb_midl_alloc(MDB_IDL_UM_MAX + 10);
	ASSERT(ids != NULL, "alloc large");
	ASSERT(ids[-1] == MDB_IDL_UM_MAX + 10, "initial large capacity");

	mdb_midl_shrink(&ids);
	ASSERT(ids[-1] == MDB_IDL_UM_MAX, "shrunk to UM_MAX if larger");

	mdb_midl_free(ids);
	ASSERT_BYTES(0);
}

/* Append list test */
Test(midl_append_list) {
	MDB_IDL ids1, ids2;
	i32 rc;

	ids1 = mdb_midl_alloc(3);
	ids2 = mdb_midl_alloc(2);

	mdb_midl_append(&ids1, 10);
	mdb_midl_append(&ids1, 20);

	mdb_midl_append(&ids2, 30);
	mdb_midl_append(&ids2, 40);

	rc = mdb_midl_append_list(&ids1, ids2);
	ASSERT(rc == 0, "append list success");
	ASSERT(ids1[0] == 4, "combined count");
	ASSERT(ids1[3] == 30, "appended value 1");
	ASSERT(ids1[4] == 40, "appended value 2");

	mdb_midl_free(ids1);
	mdb_midl_free(ids2);
	ASSERT_BYTES(0);
}

/* xmerge test */
Test(midl_xmerge) {
	MDB_IDL idl, merge;

	idl = mdb_midl_alloc(3);
	merge = mdb_midl_alloc(2);

	/* Assume sorted descending */
	idl[0] = 3;
	idl[1] = 50;
	idl[2] = 30;
	idl[3] = 10;

	merge[0] = 2;
	merge[1] = 40;
	merge[2] = 20;

	mdb_midl_xmerge(idl, merge);
	ASSERT(idl[0] == 5, "merged count");
	ASSERT(idl[1] == 50, "merged [1]");
	ASSERT(idl[2] == 40, "merged [2]");
	ASSERT(idl[3] == 30, "merged [3]");
	ASSERT(idl[4] == 20, "merged [4]");
	ASSERT(idl[5] == 10, "merged [5]");

	mdb_midl_free(idl);
	mdb_midl_free(merge);
	ASSERT_BYTES(0);
}

/* mid2l functions test */
Test(mid2l_insert_append_search) {
	MDB_ID2L ids;
	MDB_ID2 id1, id2, id3;
	u32 pos;
	i32 rc;

	/* mdb_mid2l is for MDB_ID2L, which is array of {mid, mptr} */
	/* ids[0].mid is count? From code: in append: ids[0].mid++,
	 * ids[ids[0].mid] = *id */
	/* So ids[0].mid = count, ids[1..] are entries. Assume sorted by mid
	 * descending? */

	ids = (MDB_ID2L)alloc((MDB_IDL_UM_MAX + 1) *
			      sizeof(MDB_ID2)); /* Simulate alloc, since no
				       specific alloc for ID2L */
	ids[0].mid = 0;
	ids[0].mptr = NULL; /* unused */

	id1.mid = 30;
	id1.mptr = (void *)0x30;
	id2.mid = 10;
	id2.mptr = (void *)0x10;
	id3.mid = 20;
	id3.mptr = (void *)0x20;

	rc = mdb_mid2l_append(ids, &id1);
	ASSERT(rc == 0, "append 1");
	ASSERT(ids[0].mid == 1, "count 1");
	ASSERT(ids[1].mid == 30, "value 1");

	rc = mdb_mid2l_insert(ids,
			      &id2); /* insert should binary search and shift */
	ASSERT(rc == 0, "insert 10");
	ASSERT(ids[0].mid == 2, "count 2");
	/* Assuming insert maintains order, but which order? Search is similar
	 * to mdb_midl_search, CMP(id, ids[cursor].mid)? No: val = CMP(id,
	 * ids[cursor].mid) = id < ids.mid ? -1 : >1 */
	/* Wait, CMP(x,y) ((x)<(y)?-1:(x)>(y)) so incomplete, assumes ==0 if not
	 * < or > */
	/* Then if val<0 (id < ids[cursor].mid), n=pivot left */
	/* val>0 id > ids.mid, go right */
	/* So assuming ascending? If sorted ascending, small left. For found ==,
	 * return cursor */
	/* If not, if val>0 (last id > ids.mid, meaning ids.mid < id), ++cursor
	 */
	/* Standard binsearch for ascending. */
	/* But in LMDB, mid2l is for meta pages? Wait, actually in LMDB, the
	 * ID2L is for dirty pages, sorted by page num ascending I think. */
	/* Confirm: in mdb_mid2l_insert, search, if found -1, else insert by
	 * shifting higher to right. */
	/* for(i=ids[0].mid; i>x; i--) ids[i]=ids[i-1]; so assuming ascending,
	 * since x is insert point, shift higher indices. */
	/* Yes, ascending. */

	/* After append 30, ids[1]=30 */
	/* Insert 10, search: n=1, pivot=0, cursor=1, val= CMP(10,30)= -1
	 * (10<30), n=0 */
	/* val<0, so val=-1 <0, ++cursor no, if val>0 ++cursor, but val<0,
	 * cursor=1 */
	/* Wait, after loop, if val >0 ++cursor */
	/* Since val=-1 <0, cursor=1, but since n=0 ended, and for insert, x=1,
	 * but if x<=ids[0].mid && == return -1, but not == */
	/* Then shift from 2 >1, but count=1, ++count=2, shift i=2 >1:
	 * ids[2]=ids[1], then ids[1]=id2=10 */
	/* Yes, now [10,30] */

	pos = mdb_mid2l_search(ids, 10);
	ASSERT(pos == 1, "search 10");

	pos = mdb_mid2l_search(ids, 30);
	ASSERT(pos == 2, "search 30");

	pos = mdb_mid2l_search(ids, 20);
	ASSERT(pos == 2,
	       "search 20 insert point"); /* since 10<20<30, after 1 */

	rc = mdb_mid2l_insert(ids, &id3); /* 20 */
	ASSERT(rc == 0, "insert 20");
	ASSERT(ids[0].mid == 3, "count 3");
	ASSERT(ids[1].mid == 10, "[1]=10");
	ASSERT(ids[2].mid == 20, "[2]=20");
	ASSERT(ids[3].mid == 30, "[3]=30");

	ASSERT(ids[1].mptr == (void *)0x10, "ptr1");
	ASSERT(ids[2].mptr == (void *)0x20, "ptr2");
	ASSERT(ids[3].mptr == (void *)0x30, "ptr3");

	/* Test duplicate insert */
	rc = mdb_mid2l_insert(ids, &id3);
	ASSERT(rc == -1, "duplicate insert fails");

	/* Test append too big */
	ids[0].mid = MDB_IDL_UM_MAX;
	rc = mdb_mid2l_append(ids, &id1);
	ASSERT(rc == -2, "append too big");

	release(ids); /* since manual alloc */
	ASSERT_BYTES(0);
}

Test(midl_append_range_grow) {
	MDB_IDL ids;
	i32 rc;
	ids = mdb_midl_alloc(5);
	ASSERT(ids != NULL, "alloc success");
	ASSERT(ids[-1] == 5, "initial capacity");

	rc = mdb_midl_append_range(&ids, 100, 6); /* 0 + 6 > 5, trigger grow */
	ASSERT(rc == 0, "append range grow");
	ASSERT(ids[0] == 6, "count after range");
	ASSERT(ids[1] == 105, "range value 1");
	ASSERT(ids[2] == 104, "range value 2");
	ASSERT(ids[3] == 103, "range value 3");
	ASSERT(ids[4] == 102, "range value 4");
	ASSERT(ids[5] == 101, "range value 5");
	ASSERT(ids[6] == 100, "range value 6");
	ASSERT(ids[-1] >= 5 + MDB_IDL_UM_MAX, "capacity grown");

	mdb_midl_free(ids);
	ASSERT_BYTES(0);
}
Test(midl_sort_large) {
	MDB_IDL ids;
	i32 idx;
	ids = mdb_midl_alloc(20);
	ASSERT(ids != NULL, "alloc success");

	/* Append 12 unsorted values to hit quicksort (SMALL=8) */
	for (idx = 0; idx < 12; idx++) {
		mdb_midl_append(
		    &ids, (MDB_ID)(12 - idx)); /* descending but shuffled */
	}

	ASSERT(ids[0] == 12, "count before sort");

	mdb_midl_sort(ids);
	/* Check sorted descending */
	ASSERT(ids[1] == 12, "sorted desc [1]");
	ASSERT(ids[2] == 11, "sorted desc [2]");
	ASSERT(ids[3] == 10, "sorted desc [3]");
	ASSERT(ids[4] == 9, "sorted desc [4]");
	ASSERT(ids[5] == 8, "sorted desc [5]");
	ASSERT(ids[6] == 7, "sorted desc [6]");
	ASSERT(ids[7] == 6, "sorted desc [7]");
	ASSERT(ids[8] == 5, "sorted desc [8]");
	ASSERT(ids[9] == 4, "sorted desc [9]");
	ASSERT(ids[10] == 3, "sorted desc [10]");
	ASSERT(ids[11] == 2, "sorted desc [11]");
	ASSERT(ids[12] == 1, "sorted desc [12]");

	mdb_midl_free(ids);
	ASSERT_BYTES(0);
}

Test(mid2l_insert_max) {
	MDB_ID2L ids;
	MDB_ID2 id1, id2;
	i32 rc;

	ids = (MDB_ID2L)alloc((MDB_IDL_UM_MAX + 1) * sizeof(MDB_ID2));
	ids[0].mid = MDB_IDL_UM_MAX - 1;
	ids[0].mptr = NULL;

	id1.mid = 100;
	id1.mptr = (void *)0x100;

	rc = mdb_mid2l_insert(ids, &id1);
	ASSERT(rc == 0, "insert when below max");

	id2.mid = 200; /* different mid */
	id2.mptr = (void *)0x200;

	rc = mdb_mid2l_insert(ids, &id2);
	ASSERT(rc == -2, "insert fail when at max");

	release(ids);
	ASSERT_BYTES(0);
}

/* Test for sort with large array to hit quicksort and swaps */
Test(midl_sort_quicksort) {
	MDB_IDL ids;
	i32 idx;
	MDB_ID values[20] = {
	    19, 1, 18, 2, 17, 3, 16, 4, 15, 5,
	    14, 6, 13, 7, 12, 8, 11, 9, 10, 0}; /* alternated to force swaps */

	ids = mdb_midl_alloc(20);
	ASSERT(ids != NULL, "alloc success");

	for (idx = 0; idx < 20; idx++) {
		mdb_midl_append(&ids, values[idx]);
	}

	ASSERT(ids[0] == 20, "count before sort");

	mdb_midl_sort(ids);

	/* Check sorted descending */
	for (idx = 1; idx <= 20; idx++) {
		ASSERT(ids[idx] == (u64)(20 - idx), "sorted desc check");
	}

	mdb_midl_free(ids);
	ASSERT_BYTES(0);
}
