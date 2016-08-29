/* Copyright (c) 2016, Dennis Wölfing
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

/* libc/src/stdlib/malloc.c
 * Memory allocation.
 */

#include <errno.h>
#include <stdalign.h>
#include <stddef.h>
#include "malloc.h"

void* malloc(size_t size) {
    if (size == 0) size = 1;

    size = alignUp(size, alignof(max_align_t));

    __lockHeap();

    Chunk* currentBigChunk = __firstBigChunk;
    Chunk* currentChunk = currentBigChunk + 1;

    while (1) {
        switch (currentChunk->magic) {
        case MAGIC_FREE_CHUNK:
            if (currentChunk->size >= size) {
                // We found a chunk!
                if (currentChunk->size > size + sizeof(Chunk)) {
                    // Split the chunk
                    __splitChunk(currentChunk, size);
                }
                currentChunk->magic = MAGIC_USED_CHUNK;
                __unlockHeap();
                return (void*) (currentChunk + 1);
            } else {
                currentChunk = currentChunk->next;
            }
            break;
        case MAGIC_USED_CHUNK:
            currentChunk = currentChunk->next;
            break;
        case MAGIC_END_CHUNK:
            if (currentBigChunk->next) {
                currentBigChunk = currentBigChunk->next;
                currentChunk = currentBigChunk + 1;
            } else {
                currentBigChunk = __allocateBigChunk(currentBigChunk, size);
                currentChunk = currentBigChunk + 1;
                if (!currentBigChunk) {
                    __unlockHeap();
                    errno = ENOMEM;
                    return NULL;
                }
            }
            break;
        default:
            // This shouldn't happen!
            __unlockHeap();
            return NULL;
        }
    }
}
