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

/* libc/src/stdlib/free.c
 * Memory deallocation.
 */

#include "malloc.h"

void free(void* addr) {
    if (addr == NULL) return;
    __lockHeap();

    Chunk* chunk = (Chunk*) addr - 1;

    chunk->magic = MAGIC_FREE_CHUNK;

    // Unify the chunk with its neighbors if they are free too
    if (chunk->prev && chunk->prev->magic == MAGIC_FREE_CHUNK) {
        chunk = __unifyChunks(chunk->prev, chunk);
    }

    if (chunk->next->magic == MAGIC_FREE_CHUNK) {
        chunk = __unifyChunks(chunk, chunk->next);
    }

    if (chunk->prev == NULL && chunk->next->magic == MAGIC_END_CHUNK) {
        // The whole big chunk is free, so we can unmap it
        Chunk* bigChunk = chunk - 1;
        if (bigChunk->prev) {
            bigChunk->prev->next = bigChunk->next;
        }
        if (bigChunk->next) {
            bigChunk->next->prev = bigChunk->prev;
        }
        unmapPages(bigChunk, bigChunk->size / PAGESIZE);
    }

    __unlockHeap();
}
