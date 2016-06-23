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

/* libc/src/stdlib/malloc.h
 * Internal definitions for memory allocation.
 */

#ifndef MALLOC_H
#define MALLOC_H

#include <stdlib.h>

#if __is_dennix_libc
#  include <sys/mman.h>
#  define mapPages(nPages) mmap(NULL, nPages * PAGESIZE, \
     PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
#  define unmapPages(addr, nPages) munmap(addr, nPages * PAGESIZE)
#else /* if __is_dennix_libk */
extern void* __mapPages(size_t);
extern void __unmapPages(void*, size_t);

#  define mapPages(nPages) __mapPages(nPages)
#  define unmapPages(addr, nPages) __unmapPages(addr, nPages)
#endif

typedef struct Chunk {
    size_t magic;
    size_t size;
    struct Chunk* prev;
    struct Chunk* next;
} Chunk;

#define MAGIC_BIG_CHUNK 0xC001C0DE
#define MAGIC_FREE_CHUNK 0xBEEFBEEF
#define MAGIC_USED_CHUNK 0xDEADBEEF
#define MAGIC_END_CHUNK 0xDEADDEAD

#define PAGESIZE 0x1000

#define alignUp(val, alignment) ((((val) - 1) & ~((alignment) - 1)) + (alignment))

extern Chunk* firstBigChunk;

Chunk* __allocateBigChunk(Chunk* lastBigChunk, size_t size);
void __splitChunk(Chunk* chunk, size_t size);
Chunk* __unifyChunks(Chunk* first, Chunk* second);

void __lockHeap(void);
void __unlockHeap(void);

#endif