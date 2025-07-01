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

#ifndef _RBTREE_H
#define _RBTREE_H

#include <types.H>

typedef struct RbTreeNode {
	struct RbTreeNode *parent_color;
	struct RbTreeNode *right;
	struct RbTreeNode *left;
} RbTreeNode;

typedef struct {
	RbTreeNode *parent;
	RbTreeNode *self;
	i32 is_right;
} RbTreeNodePair;

typedef struct {
	RbTreeNode *root;
} RbTree;

static const RbTree RBTREE_INIT = {NULL};

static __attribute__((unused)) void rbtree_init_node(RbTreeNode *value) {
        value->parent_color = value->right = value->left = NULL;
}



typedef i32 (*RbTreeSearch)(RbTreeNode *base, const RbTreeNode *value,
			    RbTreeNodePair *retval);

RbTreeNode *rbtree_put(RbTree *tree, RbTreeNode *value,
		       const RbTreeSearch search);
RbTreeNode *rbtree_remove(RbTree *tree, RbTreeNode *value,
			  const RbTreeSearch search);

#endif	/* _RBTREE_H */
