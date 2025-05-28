#include <bptree.h>
#include <error.h>
#include <lock.h>
#include <misc.h>
#include <socket.h>
#include <stdio.h>
#include <sys.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

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
#define MIN_PAGES (MIN_PAGE + 1)

uint64_t __bptxn_next_id = 0;

typedef struct {
	void *base;
	uint64_t capacity;
	int fd;
	uint16_t port;
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
} Metadata;

typedef struct {
	uint64_t head;
	uint64_t next_file_page;
} FreeList;

int bptree_open(BpTree *tree, const char *path) {
	/*
	byte addr[4] = {127, 0, 0, 1};
	BpTreeImpl *impl = (BpTreeImpl *)tree;
	Socket sock;
	int fd, port = 0;
	uint64_t capacity;
	void *base;

	fd = file(path);
	if (fd < 0) {
		return -1;
	}

	capacity = fsize(fd);
	if (capacity < PAGE_SIZE * MIN_PAGES) {
		err = ENOMEM;
		close(sock);
		close(fd);
		return -1;
	}
	base = fmap(fd);

	if (base == NULL) {
		close(fd);
		return -1;
	}
	impl->base = base;

	Metadata *meta1 = METADATA1(tree);
	Metadata *meta2 = METADATA2(tree);
	printf("1 %llu\n", meta1->counter);
	uint64_t counter1, counter2, desired;
	do {
		counter1 = __atomic_load_n(&meta1->counter, __ATOMIC_ACQUIRE);
		printf("2\n");
		if (counter1) break;
		int v = socket_listen(&sock, addr, 0, 1);
		if (v <= 0) continue;
		port = v;
		printf("3: %i\n", port);
		desired = 0x1 | ((uint64_t)port << 48);
	} while (__atomic_compare_exchange_n(&meta1->counter, &counter1,
					     desired, false, __ATOMIC_RELEASE,
					     __ATOMIC_RELAXED));

	if (port) {
		__atomic_store_n(&meta1->root, MIN_PAGE, __ATOMIC_RELEASE);
		__atomic_store_n(&meta1->counter, 1, __ATOMIC_RELEASE);
	} else {
	}

	impl->port = port;
	impl->fd = fd;
	impl->capacity = capacity;
	printf("port=%u\n", port);
*/
	return 0;
}
