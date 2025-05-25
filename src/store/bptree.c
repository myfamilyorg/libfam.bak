#include <bptree.h>
#include <error.h>
#include <lock.h>
#include <misc.h>
#include <sys.h>

#ifdef __linux__
#define MS_SYNC 4
#define MS_ASYNC 1
#elif defined(__APPLE__)
#define MS_SYNC 10
#define MS_ASYNC 1
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

#define TREE(txn) ((BpTreeImpl *)(((BpTxnImpl *)(txn))->tree))

#define METADATA1(txn) (Metadata *)((char *)(TREE(txn)->base))
#define METADATA2(txn) \
	(Metadata *)((char *)(((size_t)(TREE(txn))->base) + PAGE_SIZE))
#define FREE_LIST(txn) \
	(FreeList *)((char *)(((size_t)(TREE(txn))->base) + 2 * PAGE_SIZE))
#define NODE(txn, index)                                      \
	(BpTreeNode *)((char *)(((size_t)(TREE(txn))->base) + \
				index * PAGE_SIZE))

#define TREE_IS_FILE_BACKED(tree) (((BpTreeImpl *)(tree))->btype == FileBacked)
#define MIN_PAGE 3

uint64_t __bptxn_next_id = 0;

typedef struct {
	void *base;
	uint64_t capacity;
	BpTreeType btype;
} BpTreeImpl;

typedef struct {
	BpTree *tree;
	uint64_t txn_id;
	uint64_t new_root;
	uint64_t *pending_free;
	size_t pending_free_count;
	uint64_t *modified_pages;
	size_t modified_pages_count;
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

STATIC void __attribute__((constructor)) test() {
	if (sizeof(BpTree) != sizeof(BpTreeImpl)) {
		const char *s =
		    "BpTree and BpTreeImpl must have the same size!\n";
		write(2, s, strlen(s));
		exit(-1);
	}

	if (sizeof(BpTreeNode) != PAGE_SIZE) {
		const char *s =
		    "sizeof(BpTreeNode) must be equal to PAGE_SIZE!\n";
		write(2, s, strlen(s));
		exit(-1);
	}

	if (sizeof(BpTxn) != sizeof(BpTxnImpl)) {
		const char *s =
		    "BpTxn and BpTxnImpl must have the same size!\n";
		write(2, s, strlen(s));
		exit(-1);
	}
}

STATIC uint64_t bptree_allocate_node(BpTree *tree) {
	FreeList *freelist = FREE_LIST(tree);
	LockGuard lg = lock_write(&freelist->lock);
	uint64_t ret = 0, old_head = freelist->head,
		 old_next_file_page = freelist->next_file_page;
	if (freelist->head) {
		ret = freelist->head;
		BpTreeNode *node = NODE(tree, ret);
		freelist->head = node->free_list_next;
	} else if (freelist->next_file_page * PAGE_SIZE >=
		   ((BpTreeImpl *)tree)->capacity) {
		err = ENOMEM;
		return 0;
	} else {
		ret = freelist->next_file_page;
		freelist->next_file_page++;
	}
	if (TREE_IS_FILE_BACKED(tree) &&
	    msync(freelist, PAGE_SIZE, MS_ASYNC) == -1) {
		freelist->head = old_head;
		freelist->next_file_page = old_next_file_page;
		return 0;
	}
	return ret;
}

int bptree_put(BpTxn *txn, const void *key, uint16_t key_len, const void *value,
	       uint64_t value_len, const BpTreeSearch search) {
	return 0;
}
void *bptree_remove(BpTxn *txn, const void *key, uint16_t key_len,
		    const void *value, uint64_t value_len,
		    const BpTreeSearch search) {
	return NULL;
}

int bptree_init(BpTree *tree, void *base, uint64_t capacity, BpTreeType btype) {
	if (tree == NULL || base == NULL || capacity < PAGE_SIZE * 4) {
		err = EINVAL;
		return -1;
	}
	((BpTreeImpl *)tree)->btype = btype;
	((BpTreeImpl *)tree)->capacity = capacity;
	((BpTreeImpl *)tree)->base = base;

	Metadata *metadata1 = METADATA1(tree);
	Metadata *metadata2 = METADATA2(tree);
	metadata1->lock = LOCK_INIT;
	FreeList *freelist = FREE_LIST(tree);
	freelist->lock = LOCK_INIT;

	if (metadata1->counter == 0) {
		metadata1->counter = 1;
		metadata2->counter = 0;
		metadata1->root = metadata2->root = MIN_PAGE;
		FreeList *freelist = FREE_LIST(tree);
		freelist->head = 0;
		freelist->next_file_page = MIN_PAGE + 1;
		if (TREE_IS_FILE_BACKED(tree) &&
		    msync(metadata1, 2 * PAGE_SIZE, MS_SYNC) == -1) {
			metadata1->counter = 0;
			return -1;
		}
	}

	return 0;
}

void bptxn_start(BpTxn *txn, BpTree *tree) {
	((BpTxnImpl *)txn)->tree = tree;
	((BpTxnImpl *)txn)->txn_id =
	    __atomic_fetch_add(&__bptxn_next_id, 1, __ATOMIC_SEQ_CST);

	Metadata *metadata1 = METADATA1(txn);
	Metadata *metadata2 = METADATA2(txn);
	LockGuard lg = lock_read(&metadata1->lock);

	if (metadata1->counter > metadata2->counter)
		((BpTxnImpl *)txn)->new_root = metadata1->root;
	else
		((BpTxnImpl *)txn)->new_root = metadata2->root;
}
int bptxn_commit(BpTxn *txn) { return 0; }
int bptxn_abort(BpTxn *txn) { return 0; }
