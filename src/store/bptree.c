#include <bptree.h>
#include <error.h>
#include <fam.h>
#include <lock.h>
#include <misc.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"

int printf(const char *, ...);

#define TREE(txn) ((BpTreeImpl *)(((BpTxnImpl *)(txn))->tree))

#define METADATA1(tree) (((BpTreeImpl *)tree)->base)
#define METADATA2(tree) \
	(Metadata *)(PAGE_SIZE + (size_t)(((BpTreeImpl *)tree)->base))
#define FREE_LIST(tree)                                              \
	(FreeList *)((char *)(((size_t)((BpTreeImpl *)tree)->base) + \
			      2 * PAGE_SIZE))
#define NODE(tree, index)                                               \
	((BpTreePage *)((char *)(((size_t)((BpTreeImpl *)tree)->base) + \
				 index * PAGE_SIZE)))

#define COPY_NODE(tree, dst, src) \
	memcpy(NODE(tree, dst), NODE(tree, src), PAGE_SIZE)

#define IS_ROOT(tree, node_id) (NODE(tree, node_id)->parent_id == 0)

#define MIN_PAGE 3

uint64_t __bptxn_next_id = 0;

typedef struct {
	void *base;
	uint64_t capacity;
	int fd;
} BpTreeImpl;

typedef struct {
	BpTree *tree;
	uint64_t txn_id;
	uint64_t new_root;
	uint64_t *pending_free;
	size_t pending_free_count;
} BpTxnImpl;

typedef struct {
	uint64_t counter;
	uint64_t root;
	Lock lock;
} Metadata;

typedef struct {
	uint64_t head;
	uint64_t next_file_page;
	Lock lock;
} FreeList;

STATIC void __attribute__((constructor)) bptree_test(void) {
	if (sizeof(BpTree) != sizeof(BpTreeImpl)) {
		const char *s =
		    "BpTree and BpTreeImpl must have the same size!\n";
		sys_write(2, s, strlen(s));
		sys_exit(-1);
	}

	if (sizeof(BpTreePage) != PAGE_SIZE) {
		const char *s =
		    "sizeof(BpTreePage) must be equal to PAGE_SIZE!\n";
		sys_write(2, s, strlen(s));
		sys_exit(-1);
	}

	if (sizeof(BpTreeInternalPage) != sizeof(BpTreeLeafPage)) {
		const char *s =
		    "sizeof(BpTreeInternalPage) must be equal "
		    "sizeof(BpTreeLeafPage)!\n";
		sys_write(2, s, strlen(s));
		sys_exit(-1);
	}

	if (sizeof(BpTxn) != sizeof(BpTxnImpl)) {
		const char *s =
		    "BpTxn and BpTxnImpl must have the same size!\n";
		sys_write(2, s, strlen(s));
		sys_exit(-1);
	}
}

STATIC uint64_t bptree_allocate_page(BpTreeImpl *tree) {
	FreeList *freelist = FREE_LIST(tree);
	LockGuard lg = lock_write(&freelist->lock);
	uint64_t ret = 0, old_head = freelist->head,
		 old_next_file_page = freelist->next_file_page;
	if (freelist->head) {
		ret = freelist->head;
		BpTreePage *node = NODE(tree, ret);
		freelist->head = node->free_list_next;
	} else if (freelist->next_file_page * PAGE_SIZE >=
		   ((BpTreeImpl *)tree)->capacity) {
		printf("1\n");
		err = ENOMEM;
		return 0;
	} else {
		ret = freelist->next_file_page;
		freelist->next_file_page++;
	}
	// TODO: fdatasync
	return ret;
}

STATIC int bptree_add_to_node(BpTxn *txn, uint64_t node_id, uint16_t key_index,
			      const void *key, uint16_t key_len,
			      const void *value, uint32_t value_len) {
	return 0;
}

STATIC int bptree_insert_node(BpTxn *txn, const void *key, uint16_t key_len,
			      const void *value, uint64_t value_len,
			      uint64_t page_id, uint16_t key_index) {
	BpTxnImpl *impl = (BpTxnImpl *)txn;
	BpTreeImpl *tree = TREE(txn);
	uint64_t node_id = bptree_allocate_page(tree);
	printf("node_id=%lu\n", node_id);
	if (node_id == 0) return -1;

	COPY_NODE(tree, node_id, page_id);
	if (IS_ROOT(tree, page_id)) {
		impl->new_root = node_id;
		BpTreePage *nroot = NODE(tree, node_id);
		nroot->page_id = node_id;
	}
	int res = bptree_add_to_node(txn, node_id, key_index, key, key_len,
				     value, value_len);

	return res;
}

int bptree_put(BpTxn *txn, const void *key, uint16_t key_len, const void *value,
	       uint32_t value_len, const BpTreeSearch search) {
	BpTxnImpl *impl = (BpTxnImpl *)txn;
	BpTreePage *node = NODE(impl->tree, impl->new_root);
	BpTreeSearchResult result;
	printf("search with new_root=%lu, page_id=%lu\n", impl->new_root,
	       node->page_id);
	search(txn, key, key_len, node, &result);
	if (result.found) return -1;
	int res = bptree_insert_node(txn, key, key_len, value, value_len,
				     result.page_id, result.key_index);
	printf("insert-node=%i\n", res);
	return res;
}

int bptree_init(BpTree *tree, void *base, int fd, uint64_t capacity) {
	if (tree == NULL || base == NULL || capacity < PAGE_SIZE * 4) {
		err = EINVAL;
		return -1;
	}
	((BpTreeImpl *)tree)->fd = fd;
	((BpTreeImpl *)tree)->capacity = capacity;
	((BpTreeImpl *)tree)->base = base;

	Metadata *metadata1 = METADATA1(tree);
	Metadata *metadata2 = METADATA2(tree);
	metadata1->lock = LOCK_INIT;

	FreeList *freelist = FREE_LIST(tree);
	freelist->lock = LOCK_INIT;

	if (metadata1->counter == 0) {
		printf("init with counter = 0\n");
		metadata1->counter = 1;
		metadata2->counter = 0;
		metadata1->root = metadata2->root = MIN_PAGE;
		FreeList *freelist = FREE_LIST(tree);
		freelist->head = 0;
		freelist->next_file_page = MIN_PAGE + 1;

		BpTreePage *root = NODE(tree, metadata1->root);
		root->is_leaf = true;
		root->page_id = MIN_PAGE;

		sys_fdatasync(fd);
	}

	return 0;
}

BpTreePage *bptxn_get_page(BpTxn *txn, uint64_t page_id) {
	BpTxnImpl *impl = (BpTxnImpl *)txn;
	return NODE(impl->tree, page_id);
}

BpTreePage *bptree_root(BpTxn *txn) {
	BpTxnImpl *impl = (BpTxnImpl *)txn;
	return NODE(impl->tree, impl->new_root);
}

void bptxn_start(BpTxn *txn, BpTree *t) {
	((BpTxnImpl *)txn)->tree = t;
	((BpTxnImpl *)txn)->txn_id =
	    __atomic_fetch_add(&__bptxn_next_id, 1, __ATOMIC_SEQ_CST);

	BpTreeImpl *tree = TREE(txn);
	Metadata *metadata1 = METADATA1(tree);
	Metadata *metadata2 = METADATA2(tree);
	LockGuard lg = lock_read(&metadata1->lock);

	if (metadata1->counter > metadata2->counter)
		((BpTxnImpl *)txn)->new_root = metadata1->root;
	else
		((BpTxnImpl *)txn)->new_root = metadata2->root;
	printf("newr= %lu\n", ((BpTxnImpl *)txn)->new_root);
}

#pragma GCC diagnostic pop
