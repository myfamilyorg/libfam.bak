#include <bptree.h>
#include <error.h>
#include <lock.h>
#include <misc.h>
#include <sys.h>

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

STATIC void __attribute__((constructor)) bptree_test() {
	if (sizeof(BpTree) != sizeof(BpTreeImpl)) {
		const char *s =
		    "BpTree and BpTreeImpl must have the same size!\n";
		write(2, s, strlen(s));
		exit(-1);
	}

	if (sizeof(BpTreePage) != PAGE_SIZE) {
		const char *s =
		    "sizeof(BpTreePage) must be equal to PAGE_SIZE!\n";
		write(2, s, strlen(s));
		exit(-1);
	}

	if (sizeof(BpTreeInternalNode) != sizeof(BpTreeLeafNode)) {
		const char *s =
		    "sizeof(BpTreeInternalNode) must be equal "
		    "sizeof(BpTreeLeafNode)!\n";
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

		fdatasync(fd);
	}

	return 0;
}
