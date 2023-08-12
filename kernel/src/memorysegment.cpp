/* Copyright (c) 2016, 2017, 2018, 2019, 2020, 2023 Dennis Wölfing
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

/* kernel/src/memorysegment.cpp
 * Memory Segments.
 */

#include <assert.h>
#include <string.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/kthread.h>
#include <dennix/kernel/memorysegment.h>
#include <dennix/kernel/physicalmemory.h>

static char segmentsPage[PAGESIZE] ALIGNED(PAGESIZE) = {0};
static kthread_mutex_t mutex = KTHREAD_MUTEX_INITIALIZER;

static inline size_t getFreeSpaceAfter(MemorySegment::List::iterator segment) {
    auto next = Util::next(segment);
    vaddr_t nextAddress = next != MemorySegment::List::iterator{} ?
            next->address : 0;
    return nextAddress - (segment->address + segment->size);
}

void MemorySegment::addSegment(List& segments, MemorySegment* newSegment) {
    vaddr_t endAddress = newSegment->address + newSegment->size;

    for (auto iter = segments.begin(); iter != segments.end(); ++iter) {
        List::iterator next = Util::next(iter);
        if (next == segments.end() || next->address >= endAddress) {
            assert(iter->address + iter->size <= newSegment->address);
            segments.addAfter(iter, *newSegment);
            return;
        }
    }

    assert(newSegment->address == 0);
    segments.addFront(*newSegment);
}

bool MemorySegment::addSegment(List& segments, vaddr_t address,
        size_t size, int protection) {
    AutoLock lock(&mutex);
    if (!verifySegmentList()) return false;
    MemorySegment* newSegment = allocateSegment(address, size, protection);
    addSegment(segments, newSegment);
    return true;
}

MemorySegment* MemorySegment::allocateSegment(vaddr_t address, size_t size,
        int flags) {
    assert(PAGE_ALIGNED(address));
    assert(PAGE_ALIGNED(size));
    MemorySegment* current = (MemorySegment*) segmentsPage;

    while (current->address != 0 || current->size != 0) {
        current++;
        if (((uintptr_t) current & PAGE_MISALIGN) ==
                (PAGESIZE - PAGESIZE % sizeof(MemorySegment))) {
            MemorySegment** nextPage = (MemorySegment**) current;
            assert(*nextPage != nullptr);
            current = *nextPage;
        }
    }

    current->address = address;
    current->size = size;
    current->flags = flags;

    return current;
}

void MemorySegment::deallocateSegment(MemorySegment* segment) {
    memset(segment, 0, sizeof(MemorySegment));
}

void MemorySegment::removeSegment(List& segments, vaddr_t address,
        size_t size) {
    AutoLock lock(&mutex);
    vaddr_t endAddress = address + size;

    auto currentSegment = Util::findIf(segments.begin(), segments.end(),
            [address](const auto& seg) {
                return seg.address + seg.size > address ||
                        seg.address + seg.size == 0;
            });

    while (size && currentSegment != segments.end()) {
        if (currentSegment->address > address) {
            if (currentSegment->address > endAddress && endAddress != 0) {
                return;
            }
            size -= currentSegment->address - address;
            address = currentSegment->address;
        }

        if (currentSegment->address == address &&
                currentSegment->size <= size) {
            // Delete the whole segment
            address += currentSegment->size;
            size -= currentSegment->size;

            List::iterator oldSegment = currentSegment;
            ++currentSegment;
            segments.remove(*oldSegment);
            deallocateSegment(&*oldSegment);
            continue;
        } else if (currentSegment->address == address &&
                currentSegment->size > size) {
            currentSegment->address += size;
            currentSegment->size -= size;
            size = 0;
        } else if (size + (address - currentSegment->address) >=
                currentSegment->size) {
            size_t diff = currentSegment->address + currentSegment->size -
                    address;
            currentSegment->size -= diff;
            size -= diff;
            address += diff;
        } else {
            if (!verifySegmentList()) {
                // We are so low on memory that we cannot keep track of segments
                // and therefore have to leak virtual memory.
                return;
            }

            // Split the segment
            size_t firstSize = address - currentSegment->address;
            size_t secondSize = currentSegment->size - firstSize - size;

            MemorySegment* newSegment = allocateSegment(endAddress, secondSize,
                    currentSegment->flags);

            segments.addAfter(currentSegment, *newSegment);
            currentSegment->size = firstSize;
            return;
        }

        ++currentSegment;
    }
}

MemorySegment::List::iterator MemorySegment::findFreeSegment(List& segments,
        size_t size) {
    for (auto iter = segments.begin(); iter != segments.end(); ++iter) {
        if (getFreeSpaceAfter(iter) >= size) {
            return iter;
        }
    }
    return segments.end();
}

vaddr_t MemorySegment::findAndAddNewSegment(List& segments,
        size_t size, int protection) {
    AutoLock lock(&mutex);

    if (!verifySegmentList()) return 0;
    auto segment = findFreeSegment(segments, size);
    if (segment == segments.end()) return 0;

    vaddr_t address = segment->address + segment->size;
    if (segment->flags == protection) {
        segment->size += size;
        return address;
    }

    MemorySegment* newSegment = allocateSegment(address, size, protection);
    addSegment(segments, newSegment);
    return address;
}

bool MemorySegment::verifySegmentList() {
    MemorySegment* current = (MemorySegment*) segmentsPage;
    MemorySegment** nextPage;

    int freeSegmentSpaceFound = 0;
    MemorySegment* freeSegment = nullptr;

    while (true) {
        if (current->address == 0 && current->size == 0) {
            freeSegment = current;
            freeSegmentSpaceFound++;
        }

        current++;
        if (((uintptr_t) current & PAGE_MISALIGN) ==
                (PAGESIZE - PAGESIZE % sizeof(MemorySegment))) {
            nextPage = (MemorySegment**) current;
            if (!*nextPage) break;
            current = *nextPage;
        }
    }

    assert(freeSegmentSpaceFound > 0);

    if (freeSegmentSpaceFound == 1) {
        auto segment = findFreeSegment(kernelSpace->segments, PAGESIZE);
        if (segment == kernelSpace->segments.end()) return false;
        vaddr_t address = segment->address + segment->size;
        paddr_t physical = PhysicalMemory::popPageFrame();
        if (!physical) return false;
        if (!kernelSpace->mapAt(address, physical, PROT_READ | PROT_WRITE)) {
            PhysicalMemory::pushPageFrame(physical);
            return false;
        }
        *nextPage = (MemorySegment*) address;

        memset(*nextPage, 0, PAGESIZE);

        if (segment->flags == (PROT_READ | PROT_WRITE)) {
            segment->size += PAGESIZE;
        } else {
            freeSegment->address = address;
            freeSegment->size = PAGESIZE;
            freeSegment->flags = PROT_READ | PROT_WRITE;
            addSegment(kernelSpace->segments, freeSegment);
        }
    }

    return true;
}
