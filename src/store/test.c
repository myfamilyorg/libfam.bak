
#include <bptree.h>
#include <criterion/criterion.h>
#include <string.h>
#include <sys.h>
#include <sys/mman.h>
#include <types.h>

// Constants
#define O_RDWR 0x0002
#define O_CREAT 0x0040
#define MODE_0644 0644

int test_search(Txn *txn, const void *key, uint16_t key_len,
		const BpTreeNode *node, BpTreeNodePair *retval) {
	printf("search %llu\n", txn->new_root);
	retval->self_page_id = txn->new_root;
	retval->parent_page_id = txn->new_root;
	BpTreeNode *n = NODE(txn->tree, txn->new_root);
	retval->key_index = n->data.leaf.num_entries;
	printf("numentries=%u\n", n->data.leaf.num_entries);
	for (int i = 0; i < n->data.leaf.num_entries; i++) {
		uint16_t offset = n->data.leaf.entry_offsets[i];
		uint16_t cmp_len;
		uint8_t *addr = n->data.leaf.entries + offset;
		memcpy(&cmp_len, addr, sizeof(uint16_t));
		const char *cmp_key = addr + 2;
		int res;
		if (cmp_len < key_len)
			res = strncmp(key, cmp_key, cmp_len);
		else
			res = strncmp(key, cmp_key, key_len);
		if (res == 0 && key_len != cmp_len)
			res = key_len < cmp_len ? -1 : 1;
		printf("i=%i,len=%u,offset=%u,res=%i\n", i, cmp_len, offset,
		       res);
		if (res < 0) {
			retval->key_index = i;
			printf("break!");
			break;
		}
	}
	printf("----------------------------------retval->key_index=%i\n",
	       retval->key_index);
	return 0;
}

Test(core, store1) {
	remove("/tmp/store1.dat");
	int fd = open("/tmp/store1.dat", O_RDWR | O_CREAT, MODE_0644);
	cr_assert(fd >= 0);

	off_t file_size = lseek(fd, 0, SEEK_END);
	cr_assert_eq(file_size, 0);

	int ftruncate_res = ftruncate(fd, 16384 * 1024);
	cr_assert(ftruncate_res == 0);

	file_size = lseek(fd, 0, SEEK_END);
	cr_assert_eq(file_size, 16384 * 1024);

	char *base =
	    mmap(NULL, 16384 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	cr_assert(base != MAP_FAILED);

	BpTree tree = {.base = base, .fd = fd};
	bptree_init(&tree);
	Txn txn = {.tree = &tree, .new_root = 0};
	/*
	cr_assert(!bptree_put(&txn, "test1a", 6, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "abcd12", 6, "456", 3, test_search));
	cr_assert(!bptree_put(&txn, "xyzd123", 7, "456", 3, test_search));
	cr_assert(!bptree_put(&txn, "aba", 3, "oko", 3, test_search));
	cr_assert(!bptree_put(&txn, "bbbbbb", 6, "1oko", 4, test_search));
	cr_assert(!bptree_put(&txn, "yyyy", 4, "hi", 2, test_search));
	cr_assert(!bptree_put(&txn, "zzzz", 4, "hy", 2, test_search));
	cr_assert(!bptree_put(&txn, "aaa", 3, "ok1", 3, test_search));
	*/

	cr_assert(!bptree_put(&txn, "a0123456789", 11, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "b012345678", 10, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "c01234567", 9, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "d0123456", 8, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "e012345", 7, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "f01234", 6, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "g0123", 5, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "h012", 4, "123", 3, test_search));

	cr_assert(!bptree_put(&txn, "e1", 2, "11111", 5, test_search));
	cr_assert(!bptree_put(&txn, "i01", 3, "123", 3, test_search));

	close(fd);
	remove("/tmp/store1.dat");
}
