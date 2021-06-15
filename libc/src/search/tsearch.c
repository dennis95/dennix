/* Copyright (c) 2021 Dennis Wölfing
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* libc/src/search/tsearch.c
 * Find or insert an element in a binary search tree.
 */

#include "tnode.h"
#include <stdlib.h>

/*
Our binary search tree is implemented as a left-leaning red-black tree [1].
In between operations that change the tree, the following invariants hold:
1. A red node has only black children.
2. Any path from the root to any leaf crosses the same number of baclk nodes.
3. A black node can have at most one red child and this child (if it exists) is
   always the left one.

During operations that change the tree these invariants may temporarily be
violated but these violations will always be fixed before returning.

[1] Robert Sedgewick, Left-leaning Red-Black Trees (2008)
    https://www.cs.princeton.edu/~rs/talks/LLRB/LLRB.pdf
*/

static posix_tnode* search(const void* key, posix_tnode** root,
        int (*compare)(const void*, const void*)) {
    posix_tnode* node = *root;
    if (!node) {
        node = malloc(sizeof(posix_tnode));
        if (!node) return NULL;
        node->key = key;
        node->left = NULL;
        node->right = NULL;
        node->color = RED;

        *root = node;
        return node;
    }

    int cmp = compare(key, node->key);
    posix_tnode* result;

    if (cmp < 0) {
        result = search(key, &node->left, compare);
    } else if (cmp > 0) {
        result = search(key, &node->right, compare);
    } else {
        return node;
    }
    if (!result) return NULL;

    // After a new node has been inserted into the tree we might no longer have
    // a left-leaning red-black tree. We are fixing this on the way back up.
    *root = __tdelete_ensureInvariants(node);
    return result;
}

posix_tnode* tsearch(const void* key, posix_tnode** root,
        int (*compare)(const void*, const void*)) {
    if (!root) return NULL;
    posix_tnode* result = search(key, root, compare);
    (*root)->color = BLACK;
    return result;
}

posix_tnode* __tdelete_ensureInvariants(posix_tnode* node) {
    if (node->right && node->right->color == RED &&
            (!node->left || node->left->color == BLACK)) {
        // Make sure that the tree is left-leaning.
        node = rotateLeft(node);
    }

    if (node->left && node->left->color == RED &&
            node->left->left && node->left->left->color == RED) {
        // Make sure that there are no double-red paths.
        node = rotateRight(node);
    }

    if (node->left && node->left->color == RED &&
            node->right && node->right->color == RED) {
        // Make sure that the node has no two red children.
        flipColors(node);
    }

    return node;
}
