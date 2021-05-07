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

/* libc/src/search/tfind.c
 * Find an element in a binary search tree.
 */

#include "tnode.h"
#include <stddef.h>

posix_tnode* tfind(const void* key, posix_tnode* const* root,
        int (*compare)(const void*, const void*)) {
    if (!root) return NULL;
    posix_tnode* node = *root;

    while (node) {
        int cmp = compare(key, node->key);
        if (cmp < 0) {
            node = node->left;
        } else if (cmp > 0) {
            node = node->right;
        } else {
            return node;
        }
    }

    return NULL;
}
