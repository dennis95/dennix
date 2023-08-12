/* Copyright (c) 2016, 2017, 2019, 2020, 2023 Dennis Wölfing
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

#include <dennix/kernel/list.h>

#define SEG_NOUNMAP (1 << 16)

class MemorySegment {
public:
    MemorySegment(vaddr_t address, size_t size, int flags) : address{address},
            size{size}, flags{flags} {}
public:
    vaddr_t address;
    size_t size;
    int flags;
private:
    MemorySegment* prev = nullptr;
    MemorySegment* next = nullptr;
public:
    using List = LinkedList<MemorySegment, &MemorySegment::prev,
            &MemorySegment::next>;

    static bool addSegment(List& segments, vaddr_t address,
            size_t size, int protection);
    static void deallocateSegment(MemorySegment* segment);
    static void removeSegment(List& segments, vaddr_t address, size_t size);
    static vaddr_t findAndAddNewSegment(List& segments,
            size_t size, int protection);
private:
    static void addSegment(List& segments, MemorySegment* newSegment);
    static MemorySegment* allocateSegment(vaddr_t address, size_t size,
            int flags);
    static List::iterator findFreeSegment(List& segments, size_t size);
    static bool verifySegmentList();
};

#endif
