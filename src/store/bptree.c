#include <bptree.h>
#include <error.h>
#include <lock.h>
#include <misc.h>
#include <sys.h>

int printf(const char *, ...);

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

#define METADATA1(tree) (((BpTreeImpl *)tree)->base)
#define METADATA2(tree) \
	(Metadata *)(PAGE_SIZE + (size_t)(((BpTreeImpl *)tree)->base))
#define FREE_LIST(tree)                                              \
	(FreeList *)((char *)(((size_t)((BpTreeImpl *)tree)->base) + \
			      2 * PAGE_SIZE))
#define NODE(tree, index)                                               \
	((BpTreeNode *)((char *)(((size_t)((BpTreeImpl *)tree)->base) + \
				 index * PAGE_SIZE)))

#define TREE_IS_FILE_BACKED(tree) (((BpTreeImpl *)(tree))->btype == FileBacked)
#define MIN_PAGE 3

#define COPY_NODE(tree, dst, src) \
	memcpy(NODE(tree, dst), NODE(tree, src), PAGE_SIZE)

#define IS_ROOT(tree, node_id) (NODE(tree, node_id)->parent_id == 0)

#define COPY_KEY_LEAF(leaf, key, key_len, key_index)                    \
	do {                                                            \
		memcpy(leaf->entries + leaf->entry_offsets[key_index],  \
		       &key_len, sizeof(uint16_t));                     \
		memcpy(leaf->entries + leaf->entry_offsets[key_index] + \
			   sizeof(uint16_t),                            \
		       key, key_len);                                   \
	} while (false);
#define COPY_KEY_VALUE_LEAF(leaf, key, key_len, value, value_len, key_index) \
	do {                                                                 \
		COPY_KEY_LEAF(leaf, key, key_len, key_index);                \
		memcpy(leaf->entries + leaf->entry_offsets[key_index] +      \
			   sizeof(uint16_t) + key_len,                       \
		       &value_len, sizeof(uint32_t));                        \
		memcpy(leaf->entries + leaf->entry_offsets[key_index] +      \
			   sizeof(uint32_t) + sizeof(uint16_t) + key_len,    \
		       value, value_len);                                    \
	} while (false);

#define SHIFT_LEAF(node, needed, key_index)                                    \
	do {                                                                   \
		BpTreeLeafNode *_leaf__ = &node->data.leaf;                    \
		uint64_t move_len =                                            \
		    node->used_bytes - _leaf__->entry_offsets[key_index];      \
		memmove(_leaf__->entries + _leaf__->entry_offsets[key_index] + \
			    needed,                                            \
			_leaf__->entries + _leaf__->entry_offsets[key_index],  \
			move_len);                                             \
		uint16_t noffsets[MAX_ENTRIES];                                \
		for (int i = key_index + 1; i <= node->num_entries; i++)       \
			noffsets[i] = _leaf__->entry_offsets[i - 1] + needed;  \
		for (int i = key_index + 1; i <= node->num_entries; i++)       \
			_leaf__->entry_offsets[i] = noffsets[i];               \
	} while (false);

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

STATIC void __attribute__((constructor)) bptree_test() {
	printf("sizeof(BpTreeNode)=%lu %lu %lu\n", sizeof(BpTreeNode),
	       sizeof(BpTreeLeafNode), sizeof(BpTreeInternalNode));
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

STATIC uint64_t bptree_allocate_node(BpTreeImpl *tree) {
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
		printf("1\n");
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
		printf("2\n");
		return 0;
	}
	return ret;
}

STATIC uint16_t bptree_find_midpoint(BpTxn *txn, uint64_t node_id,
				     uint16_t key_index, uint64_t needed) {
	BpTxnImpl *impl = (BpTxnImpl *)txn;
	BpTreeNode *node = NODE(impl->tree, node_id);
	BpTreeLeafNode *leaf = &node->data.leaf;
	BpTreeImpl *tree = TREE(txn);

	// find point closest to midpoint after insertion
	// if we can split without overflow do so
	// if overflow is needed, use overflow for the newly inserted entry
	uint16_t ideal_midpoint = ENTRY_ARRAY_SIZE / 2;
	uint16_t num_entries = node->num_entries;
	uint16_t *entry_offsets = node->data.leaf.entry_offsets;
	printf("=========ideal_midpoint %u, num=%u, key_index=%u\n",
	       ideal_midpoint, num_entries + 1, key_index);
	uint16_t noffsets[num_entries + 1];
	uint16_t j = 0;
	for (int i = 0; i < num_entries; i++) {
		if (j == key_index) {
			noffsets[j++] = entry_offsets[i];
			noffsets[j++] = entry_offsets[i] + needed;
		} else if (j > key_index) {
			noffsets[j++] = entry_offsets[i] + needed;
		} else {
			noffsets[j++] = entry_offsets[i];
		}
		printf("entry_off(pre)[%i]=%u\n", i, entry_offsets[i]);
	}
	if (key_index == num_entries) noffsets[num_entries] = node->used_bytes;
	for (int i = 0; i < num_entries + 1; i++)
		printf("offsets[%i]=%u\n", i, noffsets[i]);

	uint16_t mid_point_index = 0;
	uint16_t left = 0;
	uint16_t right = num_entries;

	if (noffsets[right] < ideal_midpoint)
		// edge case: the far right entry is still less than the ideal
		// midpoint. We use it
		mid_point_index = right;
	else {
		// otherwise iterate through
		while (left < right) {
			uint16_t mid = left + (right - left) / 2;
			if (mid == 0) {
				if (num_entries >= 1 &&
				    noffsets[1] <= ideal_midpoint) {
					mid_point_index = 1;
				} else {
					mid_point_index = 0;
				}
				break;
			}

			if (noffsets[mid - 1] > ideal_midpoint) {
				right = mid;
			} else if (noffsets[mid] < ideal_midpoint) {
				left = mid + 1;
			} else {
				uint16_t diff_mid =
				    noffsets[mid] - ideal_midpoint;
				uint16_t diff_mid_minus_1 =
				    ideal_midpoint - noffsets[mid - 1];
				if (diff_mid < diff_mid_minus_1)
					mid_point_index = mid;
				else
					mid_point_index = mid - 1;
				break;
			}
		}
	}
	return mid_point_index;
}

STATIC int bptree_split_add(BpTxn *txn, uint64_t node_id, uint16_t key_index,
			    const void *key, uint16_t key_len,
			    const void *value, uint32_t value_len,
			    uint64_t needed) {
	BpTxnImpl *impl = (BpTxnImpl *)txn;
	BpTreeNode *node = NODE(impl->tree, node_id);
	BpTreeLeafNode *leaf = &node->data.leaf;
	BpTreeImpl *tree = TREE(txn);
	uint16_t mid_point_index =
	    bptree_find_midpoint(txn, node_id, key_index, needed);
	printf("mid_point_index=%u\n", mid_point_index);

	uint64_t split_id = bptree_allocate_node(tree);
	BpTreeNode *split = NODE(impl->tree, split_id);

	return 0;
}

STATIC int bptree_add_to_node(BpTxn *txn, uint64_t node_id, uint16_t key_index,
			      const void *key, uint16_t key_len,
			      const void *value, uint32_t value_len) {
	BpTxnImpl *impl = (BpTxnImpl *)txn;
	BpTreeNode *node = NODE(impl->tree, node_id);
	BpTreeImpl *tree = TREE(txn);

	uint64_t needed =
	    key_len + value_len + sizeof(uint32_t) + sizeof(uint16_t);

	BpTreeLeafNode *leaf = &node->data.leaf;
	if (needed > ENTRY_ARRAY_SIZE) {
		// TODO: handle overflow
	} else if (node->used_bytes + needed < ENTRY_ARRAY_SIZE &&
		   node->num_entries < MAX_ENTRIES) {
		if (key_index < node->num_entries) {
			printf("shift ki=%i ent=%i\n", key_index,
			       node->num_entries);
			SHIFT_LEAF(node, needed, key_index);
		} else {
			printf("noshift\n");
			leaf->entry_offsets[key_index] = node->used_bytes;
		}
		COPY_KEY_VALUE_LEAF(leaf, key, key_len, value, value_len,
				    key_index);
		node->used_bytes += needed;
		node->num_entries++;
		return 0;
	} else {
		return bptree_split_add(txn, node_id, key_index, key, key_len,
					value, value_len, needed);
	}
}

STATIC int bptree_insert_node(BpTxn *txn, const void *key, uint16_t key_len,
			      const void *value, uint64_t value_len,
			      uint64_t page_id, uint16_t key_index) {
	BpTxnImpl *impl = (BpTxnImpl *)txn;
	BpTreeImpl *tree = TREE(txn);
	uint64_t node_id = bptree_allocate_node(tree);
	printf("node_id=%lu\n", node_id);
	if (node_id == 0) return -1;

	COPY_NODE(tree, node_id, page_id);
	if (IS_ROOT(tree, page_id)) {
		impl->new_root = node_id;
		BpTreeNode *nroot = NODE(tree, node_id);
		nroot->page_id = node_id;
	}
	int res = bptree_add_to_node(txn, node_id, key_index, key, key_len,
				     value, value_len);

	return res;
}

BpTreeNode *bptxn_get_node(BpTxn *txn, uint64_t node_id) {
	BpTxnImpl *impl = (BpTxnImpl *)txn;
	return NODE(impl->tree, node_id);
}

BpTreeNode *bptree_root(BpTxn *txn) {
	BpTxnImpl *impl = (BpTxnImpl *)txn;
	return NODE(impl->tree, impl->new_root);
}

int bptree_put(BpTxn *txn, const void *key, uint16_t key_len, const void *value,
	       uint32_t value_len, const BpTreeSearch search) {
	BpTxnImpl *impl = (BpTxnImpl *)txn;
	BpTreeNode *node = NODE(impl->tree, impl->new_root);
	BpTreeNodeSearchResult result;
	printf("search with new_root=%lu, page_id=%lu\n", impl->new_root,
	       node->page_id);
	search(txn, key, key_len, node, &result);
	if (result.found) return -1;
	int res = bptree_insert_node(txn, key, key_len, value, value_len,
				     result.page_id, result.key_index);
	printf("insert-node=%i\n", res);
	return res;
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
		printf("init with counter = 0\n");
		metadata1->counter = 1;
		metadata2->counter = 0;
		metadata1->root = metadata2->root = MIN_PAGE;
		FreeList *freelist = FREE_LIST(tree);
		freelist->head = 0;
		freelist->next_file_page = MIN_PAGE + 1;

		BpTreeNode *root = NODE(tree, metadata1->root);
		root->is_leaf = true;
		root->page_id = MIN_PAGE;

		if (TREE_IS_FILE_BACKED(tree) &&
		    msync(metadata1, 2 * PAGE_SIZE, MS_SYNC) == -1) {
			metadata1->counter = 0;
			return -1;
		}
	}

	return 0;
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
int bptxn_commit(BpTxn *txn) { return 0; }
int bptxn_abort(BpTxn *txn) { return 0; }

