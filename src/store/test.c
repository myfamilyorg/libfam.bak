#include <bptree.h>
#include <criterion/criterion.h>
#include <misc.h>
#include <stdio.h>
#include <sys/mman.h>

#define READ_KEY_LEAF(txn, node, index) \
	KEY_PTR_LEAF(((BpTreeNode *)bptxn_get_node(&txn, node)), index)
#define READ_KEY_LEN_LEAF(txn, node, index) \
	KEY_PTR_LEN_LEAF(((BpTreeNode *)bptxn_get_node(&txn, node)), index)
#define READ_VALUE_LEAF(txn, node, index) \
	VALUE_PTR_LEAF(((BpTreeNode *)bptxn_get_node(&txn, node)), index)
#define READ_VALUE_LEN_LEAF(txn, node, index) \
	VALUE_PTR_LEN_LEAF(((BpTreeNode *)bptxn_get_node(&txn, node)), index)

void test_search(BpTxn *txn, const void *key, uint16_t key_len,
		 const BpTreeNode *node, BpTreeNodeSearchResult *retval) {
	printf("test_search %lu\n", node->page_id);
	while (!node->is_leaf) {
		printf("1\n");
		node = bptxn_get_node(txn, node->data.internal.children[0]);
		break;
	}

	printf("count=%u\n", node->num_entries);
	retval->key_index = node->num_entries;
	retval->page_id = node->page_id;
	retval->found = false;
	for (uint16_t i = 0; i < node->num_entries; i++) {
		const char *key_ptr = KEY_PTR_LEAF(node, i);
		uint16_t key_ptr_len = KEY_PTR_LEN_LEAF(node, i);

		char buf[key_ptr_len + 1];
		for (int j = 0; j < key_ptr_len + 1; j++) buf[i] = 0;
		strncpy(buf, key_ptr, key_ptr_len);
		printf("key=%s\n", buf);

		int res;
		if (key_ptr_len < key_len)
			res = strncmp(key, key_ptr, key_ptr_len);
		else
			res = strncmp(key, key_ptr, key_len);
		if (res == 0 && key_len != key_ptr_len)
			res = key_len < key_ptr_len ? -1 : 1;

		if (res <= 0) {
			if (res == 0) retval->found = true;
			retval->key_index = i;
			printf("break!\n");
			break;
		}
	}
}

Test(store, store1) {
	const char *dat_file = "/tmp/store1.dat";
	size_t size = 1024 * 16384;
	remove(dat_file);
	int fd = open_create(dat_file);
	int ftruncate_res = ftruncate(fd, size);
	cr_assert(ftruncate_res == 0);

	char *base =
	    mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	cr_assert(base != MAP_FAILED);

	BpTree tree;
	cr_assert_eq(bptree_init(&tree, base, size, FileBacked), 0);

	BpTxn txn;
	bptxn_start(&txn, &tree);

	BpTreeNodeSearchResult result;
	test_search(&txn, "a0123456789", 11, bptree_root(&txn), &result);
	cr_assert(!result.found);

	cr_assert(!bptree_put(&txn, "a0123456789", 11, "123", 3, test_search));

	test_search(&txn, "a0123456789", 11, bptree_root(&txn), &result);
	cr_assert(result.found);
	printf("result.page_id=%u,result.key_index=%u\n", result.page_id,
	       result.key_index);

	cr_assert(!bptree_put(&txn, "b012345678", 10, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "c01234567", 9, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "d0123456", 8, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "e012345", 7, "12345", 5, test_search));
	cr_assert(!bptree_put(&txn, "f01234", 6, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "g0123", 5, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "h012", 4, "123", 3, test_search));
	cr_assert(!bptree_put(&txn, "e1", 2, "11111", 5, test_search));

	test_search(&txn, "a0123456789", 11, bptree_root(&txn), &result);
	cr_assert(result.found);
	printf("result.page_id=%u,result.key_index=%u\n", result.page_id,
	       result.key_index);
	test_search(&txn, "e012345", 7, bptree_root(&txn), &result);
	cr_assert(result.found);
	printf("result.page_id=%u,result.key_index=%u\n", result.page_id,
	       result.key_index);

	void *key_data = READ_KEY_LEAF(txn, result.page_id, result.key_index);
	uint16_t key_len =
	    READ_KEY_LEN_LEAF(txn, result.page_id, result.key_index);
	cr_assert_eq(key_len, 7);
	char buf[8] = {0};
	strncat(buf, key_data, key_len);
	cr_assert(!strcmp(buf, "e012345"));
	uint32_t value_len =
	    READ_VALUE_LEN_LEAF(txn, result.page_id, result.key_index);
	void *value_data =
	    READ_VALUE_LEAF(txn, result.page_id, result.key_index);
	cr_assert_eq(value_len, 5);
	char value[6] = {0};
	strncat(value, value_data, value_len);
	cr_assert(!strcmp(value, "12345"));

	// insert a few larger entries
	char big[16384];
	memset(big, '1', sizeof(big));
	cr_assert(!bptree_put(&txn, "f111", 4, big, 4096, test_search));
	cr_assert(!bptree_put(&txn, "f112", 4, big, 4096, test_search));
	cr_assert(!bptree_put(&txn, "f113", 4, big, 4096, test_search));
	printf("last insert\n");
	cr_assert(!bptree_put(&txn, "f114", 4, big, 4096, test_search));

	remove(dat_file);
}

Test(store, store2) {
	const char *dat_file = "/tmp/store2.dat";
	size_t size = 1024 * 16384;
	remove(dat_file);
	int fd = open_create(dat_file);
	int ftruncate_res = ftruncate(fd, size);
	cr_assert(ftruncate_res == 0);

	char *base =
	    mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	cr_assert(base != MAP_FAILED);

	BpTree tree;
	cr_assert_eq(bptree_init(&tree, base, size, FileBacked), 0);

	BpTxn txn;
	bptxn_start(&txn, &tree);

	BpTreeNodeSearchResult result;

	char big[16384];
	memset(big, '1', sizeof(big));

	cr_assert(!bptree_put(&txn, "aaa", 3, big, 3, test_search));
	cr_assert(!bptree_put(&txn, "bbb", 3, big, 3, test_search));
	cr_assert(!bptree_put(&txn, "ccc", 3, big, 3, test_search));
	cr_assert(!bptree_put(&txn, "ddd", 3, big, 3, test_search));
	cr_assert(!bptree_put(&txn, "abc", 3, big, 3, test_search));
	cr_assert(!bptree_put(&txn, "eee", 3, big, 3, test_search));

	cr_assert(!bptree_put(&txn, "ccc1", 4, big, 4096, test_search));
	cr_assert(!bptree_put(&txn, "ccc2", 4, big, 4096, test_search));
	cr_assert(!bptree_put(&txn, "ccc3", 4, big, 4096, test_search));
	cr_assert(!bptree_put(&txn, "bcad", 4, big, 4096, test_search));

	remove(dat_file);
}
