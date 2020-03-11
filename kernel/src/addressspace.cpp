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

/* kernel/src/addressspace.cpp
 * Address space class.
 */

#include <string.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/physicalmemory.h>

#define PAGE_PRESENT (1 << 0)
#define PAGE_WRITABLE (1 << 1)
#define PAGE_USER (1 << 2)

static AddressSpace _kernelSpace;
AddressSpace* const kernelSpace = &_kernelSpace;
AddressSpace* AddressSpace::activeAddressSpace;

bool AddressSpace::isActive() {
    return this == kernelSpace || this == activeAddressSpace;
}

static kthread_mutex_t forkMutex = KTHREAD_MUTEX_INITIALIZER;

AddressSpace* AddressSpace::fork() {
    AutoLock lock(&forkMutex);

    AddressSpace* result = new AddressSpace();
    if (!result) return nullptr;
    MemorySegment* segment = firstSegment->next;
    while (segment) {
        if (!(segment->flags & SEG_NOUNMAP)) {
            // Copy the segment
            size_t size = segment->size;
            if (!result->mapMemory(segment->address, size, segment->flags)) {
                delete result;
                return nullptr;
            }

            vaddr_t source = kernelSpace->mapFromOtherAddressSpace(this,
                    segment->address, size, PROT_READ);
            if (!source) {
                delete result;
                return nullptr;
            }

            vaddr_t dest = kernelSpace->mapFromOtherAddressSpace(result,
                    segment->address, size, PROT_WRITE);
            if (!dest) {
                kernelSpace->unmapPhysical(source, size);
                delete result;
                return nullptr;
            }

            memcpy((void*) dest, (const void*) source, size);
            kernelSpace->unmapPhysical(source, size);
            kernelSpace->unmapPhysical(dest, size);
        }
        segment = segment->next;
    }

    return result;
}

vaddr_t AddressSpace::mapFromOtherAddressSpace(AddressSpace* sourceSpace,
        vaddr_t sourceVirtualAddress, size_t size, int protection) {
    kthread_mutex_lock(&mutex);
    vaddr_t destination = MemorySegment::findAndAddNewSegment(firstSegment,
            size, protection);
    kthread_mutex_unlock(&mutex);
    if (!destination) return 0;

    for (size_t i = 0 ; i < size; i += PAGESIZE) {
        kthread_mutex_lock(&sourceSpace->mutex);
        paddr_t physicalAddress =
                sourceSpace->getPhysicalAddress(sourceVirtualAddress + i);
        kthread_mutex_unlock(&sourceSpace->mutex);
        kthread_mutex_lock(&mutex);
        if (!mapAt(destination + i, physicalAddress, protection)) {
            for (size_t j = 0; j < i; j += PAGESIZE) {
                unmap(destination + j);
            }
            MemorySegment::removeSegment(firstSegment, destination, size);
            return 0;
        }
        kthread_mutex_unlock(&mutex);
    }


    return destination;
}

vaddr_t AddressSpace::mapMemoryInternal(vaddr_t virtualAddress, size_t size,
        int protection) {
    size_t pages = size / PAGESIZE;

    if (!PhysicalMemory::reserveFrames(pages)) {
        MemorySegment::removeSegment(firstSegment, virtualAddress, size);
        return 0;
    }

    for (size_t i = 0; i < pages; i++) {
        paddr_t physicalAddress = PhysicalMemory::popReserved();
        if (unlikely(!mapAt(virtualAddress + i * PAGESIZE, physicalAddress,
                protection))) {
            PhysicalMemory::unreserveFrames(pages - i - 1);
            PhysicalMemory::pushPageFrame(physicalAddress);

            for (size_t j = 0; j < i; j++) {
                physicalAddress = getPhysicalAddress(virtualAddress +
                        j * PAGESIZE);
                unmap(virtualAddress + j * PAGESIZE);
                PhysicalMemory::pushPageFrame(physicalAddress);
            }
            MemorySegment::removeSegment(firstSegment, virtualAddress, size);
            return 0;
        }
    }

    return virtualAddress;
}

vaddr_t AddressSpace::mapMemory(size_t size, int protection) {
    AutoLock lock(&mutex);
    vaddr_t virtualAddress = MemorySegment::findAndAddNewSegment(firstSegment,
            size, protection);
    if (!virtualAddress) return 0;
    return mapMemoryInternal(virtualAddress, size, protection);
}

vaddr_t AddressSpace::mapMemory(vaddr_t virtualAddress, size_t size,
        int protection) {
    AutoLock lock(&mutex);

    if (!MemorySegment::addSegment(firstSegment, virtualAddress, size,
            protection)) {
        return 0;
    }
    return mapMemoryInternal(virtualAddress, size, protection);
}

vaddr_t AddressSpace::mapPhysical(paddr_t physicalAddress, size_t size,
        int protection) {
    AutoLock lock(&mutex);

    vaddr_t virtualAddress = MemorySegment::findAndAddNewSegment(firstSegment,
            size, protection);
    if (!virtualAddress) return 0;
    for (size_t i = 0; i < size; i += PAGESIZE) {
        if (!mapAt(virtualAddress + i, physicalAddress + i, protection)) {
            for (size_t j = 0; j < i; j += PAGESIZE) {
                unmap(virtualAddress + j);
            }
            MemorySegment::removeSegment(firstSegment, virtualAddress, size);
            return 0;
        }
    }

    return virtualAddress;
}

void AddressSpace::unmap(vaddr_t virtualAddress) {
    mapAt(virtualAddress, 0, 0);
}

void AddressSpace::unmapMemory(vaddr_t virtualAddress, size_t size) {
    AutoLock lock(&mutex);

    for (size_t i = 0; i < size; i += PAGESIZE) {
        paddr_t physicalAddress = getPhysicalAddress(virtualAddress + i);
        unmap(virtualAddress + i);
        PhysicalMemory::pushPageFrame(physicalAddress);
    }

    MemorySegment::removeSegment(firstSegment, virtualAddress, size);
}

void AddressSpace::unmapPhysical(vaddr_t virtualAddress, size_t size) {
    AutoLock lock(&mutex);

    for (size_t i = 0; i < size; i += PAGESIZE) {
        unmap(virtualAddress + i);
    }

    MemorySegment::removeSegment(firstSegment, virtualAddress, size);
}
