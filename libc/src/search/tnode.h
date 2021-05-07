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

/* libc/src/search/tnode.h
 * Binary search tree nodes.
 */

#ifndef TNODE_H
#define TNODE_H

#include <stdbool.h>

struct posix_tnode {
    const void* key;
    struct posix_tnode* left;
    struct posix_tnode* right;
    bool color;
};

#define RED true
#define BLACK false

#define _TNODE struct posix_tnode
#include <search.h>

static inline posix_tnode* rotateLeft(posix_tnode* node) {
    // Turn a right-leaning link into a left-leaning one.
    posix_tnode* right = node->right;
    node->right = right->left;
    right->left = node;
    right->color = node->color;
    node->color = RED;
    return right;
}

static inline posix_tnode* rotateRight(posix_tnode* node) {
    // Turn a left-leaning link into a right-leaning one.
    posix_tnode* left = node->left;
    node->left = left->right;
    left->right = node;
    left->color = node->color;
    node->color = RED;
    return left;
}

static inline void flipColors(posix_tnode* node) {
    node->color = !node->color;
    node->left->color = !node->left->color;
    node->right->color = !node->right->color;
}

posix_tnode* __tdelete_ensureInvariants(posix_tnode* node);

#endif
