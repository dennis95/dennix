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

/* libc/include/search.h
 * Search datastructures.
 */

#ifndef _SEARCH_H
#define _SEARCH_H

#include <sys/cdefs.h>
#define __need_size_t
#include <bits/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    preorder,
    postorder,
    endorder,
    leaf
} VISIT;

#ifndef _TNODE
#  define _TNODE void
#endif

typedef _TNODE posix_tnode;

void* tdelete(const void* __restrict, posix_tnode** __restrict,
        int (*)(const void*, const void*));
posix_tnode* tfind(const void*, posix_tnode* const*,
        int (*)(const void*, const void*));
posix_tnode* tsearch(const void*, posix_tnode**,
        int (*)(const void*, const void*));
void twalk(const posix_tnode*, void (*)(const posix_tnode*, VISIT, int));

#ifdef __cplusplus
}
#endif

#endif
