#ifndef _BTREE_H__
#define _BTREE_H__

typedef struct BTree BTree;

int btree_set(BTree *tree, const char *key, const char *value);
const char *btree_get(BTree *tree, const char *key);
const char *btree_remove(BTree *tree, const char *key);

#endif /* _BTREE_H__ */
