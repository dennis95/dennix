/* Copyright (c) 2018 Dennis Wölfing
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

/* libc/src/stdlib/bsearch.c
 * Binary search.
 */

#include <stdint.h>
#include <stdlib.h>

void* bsearch(const void* key, const void* base, size_t count, size_t size,
        int (*compare)(const void*, const void*)) {
    while (count > 0) {
        size_t index = (count - 1) / 2;
        int cmp = compare(key, (const void*) ((uintptr_t) base + index * size));
        if (cmp == 0) {
            return (void*) ((uintptr_t) base + index * size);
        } else if (cmp < 0) {
            count = index;
        } else {
            base = (const void*) ((uintptr_t) base + (index + 1) * size);
            count -= index + 1;
        }
    }

    return NULL;
}
