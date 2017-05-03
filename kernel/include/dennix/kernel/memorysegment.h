/* Copyright (c) 2016, 2017 Dennis Wölfing
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

/* kernel/include/dennix/kernel/memorysegment.h
 * Memory Segments.
 */

#ifndef KERNEL_MEMORYSEGMENT_H
#define KERNEL_MEMORYSEGMENT_H

#include <dennix/kernel/kernel.h>

#define SEG_NOUNMAP (1 << 16)

class MemorySegment {
public:
    MemorySegment(vaddr_t address, size_t size, int flags, MemorySegment* prev,
            MemorySegment* next);
public:
    vaddr_t address;
    size_t size;
    int flags;
    MemorySegment* prev;
    MemorySegment* next;
public:
    static void addSegment(MemorySegment* firstSegment, vaddr_t address,
            size_t size, int protection);
    static void removeSegment(MemorySegment* firstSegment, vaddr_t address,
            size_t size);
    static vaddr_t findAndAddNewSegment(MemorySegment* firstSegment,
            size_t size, int protection);
private:
    static void addSegment(MemorySegment* firstSegment,
            MemorySegment* newSegment);
    static MemorySegment* allocateSegment(vaddr_t address, size_t size,
            int flags);
    static void deallocateSegment(MemorySegment* segment);
    static vaddr_t findFreeSegment(MemorySegment* firstSegment, size_t size);
    static void verifySegmentList();
};

#endif
