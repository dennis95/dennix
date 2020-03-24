/* Copyright (c) 2016, 2019, 2020 Dennis Wölfing
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

/* libc/src/stdlib/realloc.c
 * Changes the size of an allocation.
 */

#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "malloc.h"

static void changeChunkSize(Chunk* chunk, ssize_t sizeDiff) {
    Chunk* next = chunk->next;

    Chunk* newNextChunk = (Chunk*) ((uintptr_t) next + sizeDiff);
    memmove(newNextChunk, next, sizeof(Chunk));
    chunk->next = newNextChunk;

    chunk->size += sizeDiff;
    newNextChunk->size -= sizeDiff;

    if (newNextChunk->next) {
        newNextChunk->next->prev = newNextChunk;
    }
}

void* realloc(void* addr, size_t size) {
    if (addr == NULL) return malloc(size);

    __lockHeap();

    Chunk* chunk = (Chunk*) addr - 1;
    assert(chunk->magic == MAGIC_USED_CHUNK);

    if (size == 0) size = 1;
    size = alignUp(size, 16);

    ssize_t sizeDiff = size - chunk->size;

    Chunk* next = chunk->next;

    if (sizeDiff == 0) {
        __unlockHeap();
        return addr;
    }

    if (next->magic == MAGIC_FREE_CHUNK) {
        // The next chunk is free so we can move its chunk header freely
        if ((sizeDiff > 0 && next->size > sizeDiff + sizeof(Chunk)) ||
                sizeDiff < 0) {
            changeChunkSize(chunk, sizeDiff);

            __unlockHeap();
            return addr;
        }
    }

    // TODO: If we could not resize the next chunk we could also try to split
    // or unify chunks to avoid copying.

    __unlockHeap();

    // Create a new chunk and copy the contents to it
    void* newAddress = malloc(size);
    if (!newAddress) return NULL;
    size_t copySize = sizeDiff < 0 ? size : chunk->size;
    memcpy(newAddress, addr, copySize);
    free(addr);
    return newAddress;
}
