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

/* libc/src/search/tdelete.c
 * Delete an element from a binary search tree.
 */

#include "tnode.h"
#include <stdlib.h>

/*
During the delete operation we descend down the tree to find the element to
delete. If that element is not a leaf we will exchange it for the smallest
element larger than it. That way we can always treat this as deleting a leaf.
During the descent we rearrange the tree to ensure that for each node that we
look at either the node is red or the next node that we will look at is red.
This means that the node being deleted will always be red and therefore the
deletion will not imbalance the tree.
*/

static posix_tnode* moveRedLeft(posix_tnode* node) {
    // Make sure that either left or left->left is red.
    flipColors(node);
    if (node->right && node->right->left && node->right->left->color == RED) {
        node->right = rotateRight(node->right);
        node = rotateLeft(node);
        flipColors(node);
    }
    return node;
}

static posix_tnode* moveRedRight(posix_tnode* node) {
    // Make sure that either right or right->right is red.
    flipColors(node);
    if (node->left && node->left->left && node->left->left->color == RED) {
        node = rotateRight(node);
        flipColors(node);
    }
    return node;
}

static const void* deleteMin(posix_tnode** restrict root,
        int (*compare)(const void*, const void*)) {
    // Delete the smallest node in the tree and return its key.
    posix_tnode* node = *root;
    if (!node->left) {
        const void* result = node->key;
        free(node);
        *root = NULL;
        return result;
    }

    if (node->left->color == BLACK &&
            (!node->left->left || node->left->left->color == BLACK)) {
        node = moveRedLeft(node);
    }

    const void* result = deleteMin(&node->left, compare);
    *root = __tdelete_ensureInvariants(node);
    return result;
}

static void* delete(const void* restrict key, posix_tnode** restrict root,
        int (*compare)(const void*, const void*)) {
    posix_tnode* node = *root;
    if (!node) return NULL;
    posix_tnode* result;

    if (compare(key, node->key) < 0) {
        if (!node->left) return NULL;
        if (node->left->color == BLACK &&
                (!node->left->left || node->left->left->color == BLACK)) {
            // Make sure that we are following red nodes.
            node = moveRedLeft(node);
        }
        result = delete(key, &node->left, compare);
        if (result == (void*) -1) {
            result = node;
        }
    } else {
        if (node->left && node->left->color == RED) {
            node = rotateRight(node);
        }

        if (compare(key, node->key) == 0 && !node->right) {
            free(node);
            *root = NULL;
            return (void*) -1;
        }

        if (!node->right) return NULL;

        if (node->right->color == BLACK &&
                (!node->right->left || node->right->left->color == BLACK)) {
            // Make sure that we are following red nodes.
            node = moveRedRight(node);
        }

        if (compare(key, node->key) == 0) {
            // Delete the smallest node larger than the current one and set the
            // current key to the deleted one.
            node->key = deleteMin(&node->right, compare);
            result = (void*) -1;
        } else {
            result = delete(key, &node->right, compare);
            if (result == (void*) -1) {
                result = node;
            }
        }
    }

    *root = __tdelete_ensureInvariants(node);
    return result;
}

void* tdelete(const void* restrict key, posix_tnode** restrict root,
        int (*compare)(const void*, const void*)) {
    if (!root) return NULL;

    void* result = delete(key, root, compare);
    if (*root) {
        (*root)->color = BLACK;
    }
    return result;
}
