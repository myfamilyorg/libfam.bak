#include <bptree.h>
#include <error.h>
#include <fcntl.h>
#include <lock.h>
#include <misc.h>
#include <sys.h>
#include <sys/mman.h>

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

STATIC uint64_t bptree_allocate_node(BpTree *tree) {
	FreeList *freelist = FREE_LIST(tree);
	LockGuard lg = lock_write(&freelist->lock);
	uint64_t ret = 0, old_head = freelist->head,
		 old_next_file_page = freelist->next_file_page;
	if (freelist->head) {
		ret = freelist->head;
		BpTreeNode *node = NODE(tree, ret);
		freelist->head = node->free_list_next;
	} else if (freelist->next_file_page >= tree->capacity) {
		err = ENOMEM;
		return 0;
	} else {
		ret = freelist->next_file_page;
		freelist->next_file_page++;
	}
	if (msync(freelist, PAGE_SIZE, MS_SYNC) == -1) {
		freelist->head = old_head;
		freelist->next_file_page = old_next_file_page;
		return 0;
	}
	return ret;
}

STATIC void bptree_set_root(Txn *txn) {
	Metadata *metadata1 = METADATA1(txn->tree);
	Metadata *metadata2 = METADATA2(txn->tree);
	LockGuard lg = lock_read(&metadata1->lock);

	if (metadata1->counter > metadata2->counter)
		txn->new_root = metadata1->root;
	else
		txn->new_root = metadata2->root;
}

int printf(const char *, ...);

STATIC int bptree_insert_at(Txn *txn, uint64_t node_id, const void *key,
			    uint16_t key_len, const void *value,
			    uint64_t value_len, uint16_t key_index) {
	BpTreeNode *n = NODE(txn->tree, node_id);
	BpTreeLeafNode *leaf = &n->data.leaf;
	uint64_t needed = NEEDED_KEY_VALUE(key_len, value_len);
	printf("insert_at needed=%u,key_index=%u,num_entries=%u\n", needed,
	       key_index, leaf->num_entries);
	if (needed + leaf->used_bytes < ENTRY_ARRAY_SIZE) {
		if (key_index < leaf->num_entries) {
			uint64_t move_len =
			    leaf->used_bytes - leaf->entry_offsets[key_index];
			memmove(leaf->entries + leaf->entry_offsets[key_index] +
				    needed,
				leaf->entries + leaf->entry_offsets[key_index],
				move_len);
			uint16_t noffsets[MAX_ENTRIES];
			for (int i = key_index + 1; i <= leaf->num_entries; i++)
				noffsets[i] =
				    leaf->entry_offsets[i - 1] + needed;

			for (int i = key_index + 1; i <= leaf->num_entries; i++)
				leaf->entry_offsets[i] = noffsets[i];
		} else
			leaf->entry_offsets[key_index] = leaf->used_bytes;
		COPY_KEY_VALUE_LEAF(leaf, key, key_len, value, value_len,
				    key_index);
		leaf->used_bytes += needed;
		leaf->num_entries++;
	} else {
		uint64_t split_id = bptree_allocate_node(txn->tree);
		if (split_id == 0) return -1;
		if (n->parent_id == 0) {
			// this is the root node we'll need to create a new root
			// to point to the split
			uint64_t nroot = bptree_allocate_node(txn->tree);
			if (nroot == 0) return -1;  // TODO: release split_id
		}
		BpTreeNode *split = NODE(txn->tree, split_id);
		split->is_leaf = true;
		split->page_id = split_id;
		split->free_list_next = 0;

		uint16_t needed_left;
		if (key_index == leaf->num_entries)
			needed_left = leaf->used_bytes;
		else
			needed_left = leaf->entry_offsets[key_index];
		uint16_t needed_right =
		    (needed + leaf->used_bytes) - needed_left;

		if (needed_left + needed < ENTRY_ARRAY_SIZE) {
			// add to left side and split off the right
			uint16_t right_count = leaf->num_entries - key_index;
			uint16_t left_count = key_index;
			split->data.leaf.num_entries =
			    leaf->num_entries - key_index;
			split->data.leaf.used_bytes = needed_right;
			memmove(split->data.leaf.entry_offsets,
				leaf->entry_offsets + left_count,
				right_count * sizeof(uint16_t));
			memmove(split->data.leaf.is_overflow,
				leaf->is_overflow + left_count,
				right_count * sizeof(uint8_t)

			);
			memmove(split->data.leaf.entries,
				leaf->entries + needed_left, needed_right);
			for (uint16_t i = 0; i < right_count; i++) {
				split->data.leaf.entry_offsets[i] -=
				    needed_left;
			}
			COPY_KEY_VALUE_LEAF(leaf, key, key_len, value,
					    value_len, key_index);
			leaf->entry_offsets[key_index] = needed_left;
			leaf->used_bytes = needed_left + needed;
			leaf->num_entries = left_count + 1;

		} else if (needed_right + needed < ENTRY_ARRAY_SIZE) {
			// add to right side and split off the left
			uint16_t right_count = leaf->num_entries - key_index;
			uint16_t left_count = key_index;
			split->data.leaf.num_entries =
			    leaf->num_entries - key_index;
			split->data.leaf.used_bytes = needed_right;
			memmove(
			    split->data.leaf.entry_offsets + sizeof(uint16_t),
			    leaf->entry_offsets + left_count,
			    right_count * sizeof(uint16_t));
			memmove(split->data.leaf.is_overflow + sizeof(uint8_t),
				leaf->is_overflow + left_count,
				right_count * sizeof(uint8_t));
			memmove(split->data.leaf.entries + needed,
				leaf->entries + needed_left, needed_right);

			for (uint16_t i = 1; i < right_count + 1; i++) {
				split->data.leaf.entry_offsets[i] -=
				    needed_left;
				split->data.leaf.entry_offsets[i] += needed;
			}
			COPY_KEY_VALUE_LEAF((&split->data.leaf), key, key_len,
					    value, value_len, 0);
			split->data.leaf.entry_offsets[0] = 0;
			split->data.leaf.used_bytes = needed_right + needed;
			split->data.leaf.num_entries = right_count + 1;
			leaf->num_entries = left_count;
			leaf->used_bytes = needed_left;
		} else {
			if (needed_left < needed_right) {
				// overflow on the left side
			} else {
				// overflow on the right side
			}
		}
	}
	return 0;
}

STATIC int bptree_insert_node(Txn *txn, const void *key, uint16_t key_len,
			      const void *value, uint64_t value_len,
			      uint64_t page_id, uint16_t key_index) {
	uint64_t node_id = bptree_allocate_node(txn->tree);
	if (node_id == 0) return -1;
	/* check if node exists and copy it if so */
	if (page_id >= MIN_PAGE)
		COPY_NODE(txn->tree, node_id, page_id);
	else {
		/* Init node */
		BpTreeNode *n = NODE(txn->tree, node_id);
		n->page_id = page_id;
		n->is_leaf = true;
		n->free_list_next = 0;
		n->data.leaf = (BpTreeLeafNode){0};
	}

	int res = bptree_insert_at(txn, node_id, key, key_len, value, value_len,
				   key_index);

	txn->new_root = node_id;
	return res;
}

int bptree_init(BpTree *tree) {
	off_t file_size = lseek(tree->fd, 0, SEEK_END);
	if (file_size < PAGE_SIZE * 2) {
		err = EFAULT;
		return -1;
	}
	Metadata *metadata1 = METADATA1(tree);
	Metadata *metadata2 = METADATA2(tree);
	metadata1->lock = LOCK_INIT;
	FreeList *freelist = FREE_LIST(tree);
	freelist->lock = LOCK_INIT;
	tree->capacity = file_size / PAGE_SIZE;

	if (metadata1->counter == 0) {
		metadata1->counter = 1;
		metadata2->counter = 0;
		// init the root pointers
		metadata1->root = metadata2->root = MIN_PAGE;
		// init the free list
		FreeList *freelist = FREE_LIST(tree);
		freelist->head = 0;
		freelist->next_file_page = MIN_PAGE + 1;
		if (msync(metadata1, 2 * PAGE_SIZE, MS_SYNC) == -1) {
			metadata1->counter = 0;
			return -1;
		}
	}
	return 0;
}

int bptree_put(Txn *txn, const void *key, uint16_t key_len, const void *value,
	       uint64_t value_len, const BpTreeSearch search) {
	int res;
	if (!txn->new_root) bptree_set_root(txn);
	BpTreeNode *node = NODE(txn->tree, txn->new_root);
	BpTreeNodePair retval;
	search(txn, key, key_len, node, &retval);
	res = bptree_insert_node(txn, key, key_len, value, value_len,
				 retval.self_page_id, retval.key_index);
	return res;
}

void *bptree_remove(Txn *txn, const void *key, uint16_t key_len,
		    const void *value, uint64_t value_len,
		    const BpTreeSearch search) {
	return NULL;
}
