/* Copyright (c) 2016, 2017, 2018, 2019, 2020 Dennis Wölfing
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

static inline size_t getFreeSpaceAfter(MemorySegment* segment) {
    vaddr_t nextAddress = segment->next ? segment->next->address : 0;
    return nextAddress - (segment->address + segment->size);
}

MemorySegment::MemorySegment(vaddr_t address, size_t size, int flags,
        MemorySegment* prev, MemorySegment* next) {
    this->address = address;
    this->size = size;
    this->flags = flags;
    this->prev = prev;
    this->next = next;
}

void MemorySegment::addSegment(MemorySegment* firstSegment,
        MemorySegment* newSegment) {
    vaddr_t endAddress = newSegment->address + newSegment->size;

    MemorySegment* currentSegment = firstSegment;

    while (currentSegment->next &&
            currentSegment->next->address < endAddress) {
        currentSegment = currentSegment->next;
    }

    assert(currentSegment->address + currentSegment->size <=
            newSegment->address);
    assert(!currentSegment->next ||
            currentSegment->next->address >= endAddress);

    newSegment->prev = currentSegment;
    newSegment->next = currentSegment->next;

    currentSegment->next = newSegment;
    if (newSegment->next) {
        newSegment->next->prev = newSegment;
    }
}

bool MemorySegment::addSegment(MemorySegment* firstSegment, vaddr_t address,
        size_t size, int protection) {
    AutoLock lock(&mutex);
    if (!verifySegmentList()) return false;
    MemorySegment* newSegment = allocateSegment(address, size, protection);
    addSegment(firstSegment, newSegment);
    return true;
}

MemorySegment* MemorySegment::allocateSegment(vaddr_t address, size_t size,
        int flags) {
    assert(PAGE_ALIGNED(address));
    assert(PAGE_ALIGNED(size));
    MemorySegment* current = (MemorySegment*) segmentsPage;

    while (current->address != 0 && current->size != 0) {
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

void MemorySegment::removeSegment(MemorySegment* firstSegment, vaddr_t address,
        size_t size) {
    AutoLock lock(&mutex);
    MemorySegment* currentSegment = firstSegment;
    vaddr_t endAddress = address + size;

    while (currentSegment &&
            currentSegment->address + currentSegment->size <= address &&
            currentSegment->address + currentSegment->size != 0) {
        currentSegment = currentSegment->next;
    }

    while (size && currentSegment) {
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

            MemorySegment* next = currentSegment->next;
            if (next) {
                next->prev = currentSegment->prev;
            }
            if (currentSegment->prev) {
                currentSegment->prev->next = next;
            }

            deallocateSegment(currentSegment);
            currentSegment = next;
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

            newSegment->prev = currentSegment;
            newSegment->next = currentSegment->next;
            if (newSegment->next) {
                newSegment->next->prev = newSegment;
            }

            currentSegment->next = newSegment;
            currentSegment->size = firstSize;
        }

        currentSegment = currentSegment->next;
    }
}

MemorySegment* MemorySegment::findFreeSegment(MemorySegment* firstSegment,
        size_t size) {
    MemorySegment* currentSegment = firstSegment;

    while (currentSegment && getFreeSpaceAfter(currentSegment) < size) {
        currentSegment = currentSegment->next;
    }

    return currentSegment;
}

vaddr_t MemorySegment::findAndAddNewSegment(MemorySegment* firstSegment,
        size_t size, int protection) {
    AutoLock lock(&mutex);

    if (!verifySegmentList()) return 0;
    MemorySegment* segment = findFreeSegment(firstSegment, size);
    if (!segment) return 0;

    vaddr_t address = segment->address + segment->size;
    if (segment->flags == protection) {
        segment->size += size;
        return address;
    }

    MemorySegment* newSegment = allocateSegment(address, size, protection);
    addSegment(firstSegment, newSegment);
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
        current = findFreeSegment(kernelSpace->firstSegment, PAGESIZE);
        if (!current) return false;
        vaddr_t address = current->address + current->size;
        paddr_t physical = PhysicalMemory::popPageFrame();
        if (!physical) return false;
        if (!kernelSpace->mapAt(address, physical, PROT_READ | PROT_WRITE)) {
            PhysicalMemory::pushPageFrame(physical);
            return false;
        }
        *nextPage = (MemorySegment*) address;

        memset(*nextPage, 0, PAGESIZE);

        if (current->flags == (PROT_READ | PROT_WRITE)) {
            current->size += PAGESIZE;
        } else {
            freeSegment->address = address;
            freeSegment->size = PAGESIZE;
            freeSegment->flags = PROT_READ | PROT_WRITE;
            addSegment(kernelSpace->firstSegment, freeSegment);
        }
    }

    return true;
}
