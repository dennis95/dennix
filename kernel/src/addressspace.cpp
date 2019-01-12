/* Copyright (c) 2016, 2017, 2018, 2019 Dennis Wölfing
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

AddressSpace AddressSpace::_kernelSpace;
AddressSpace* kernelSpace;
AddressSpace* AddressSpace::activeAddressSpace;

bool AddressSpace::isActive() {
    return this == kernelSpace || this == activeAddressSpace;
}

static kthread_mutex_t forkMutex = KTHREAD_MUTEX_INITIALIZER;

AddressSpace* AddressSpace::fork() {
    AutoLock lock(&forkMutex);

    AddressSpace* result = new AddressSpace();
    MemorySegment* segment = firstSegment->next;
    while (segment) {
        if (!(segment->flags & SEG_NOUNMAP)) {
            // Copy the segment
            size_t size = segment->size;
            result->mapMemory(segment->address, size, segment->flags);
            vaddr_t source = kernelSpace->mapFromOtherAddressSpace(this,
                    segment->address, size, PROT_READ);
            vaddr_t dest = kernelSpace->mapFromOtherAddressSpace(result,
                    segment->address, size, PROT_WRITE);
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

    for (size_t i = 0 ; i < size; i += 0x1000) {
        kthread_mutex_lock(&sourceSpace->mutex);
        paddr_t physicalAddress =
                sourceSpace->getPhysicalAddress(sourceVirtualAddress + i);
        kthread_mutex_unlock(&sourceSpace->mutex);
        kthread_mutex_lock(&mutex);
        mapAt(destination + i, physicalAddress, protection);
        kthread_mutex_unlock(&mutex);
    }


    return destination;
}

vaddr_t AddressSpace::mapMemory(size_t size, int protection) {
    AutoLock lock(&mutex);

    vaddr_t virtualAddress = MemorySegment::findAndAddNewSegment(firstSegment,
            size, protection);
    paddr_t physicalAddress;

    for (size_t i = 0; i < size; i += 0x1000) {
        physicalAddress = PhysicalMemory::popPageFrame();
        if (!physicalAddress ||
                !mapAt(virtualAddress + i, physicalAddress, protection)) {
            if (physicalAddress) {
                PhysicalMemory::pushPageFrame(physicalAddress);
            }
            for (i = i - 0x1000; i < size; i -= 0x1000) {
                physicalAddress = getPhysicalAddress(virtualAddress + i);
                unmap(virtualAddress + i);
                PhysicalMemory::pushPageFrame(physicalAddress);
            }
            MemorySegment::removeSegment(firstSegment, virtualAddress, size);
            return 0;
        }
    }

    return virtualAddress;
}

vaddr_t AddressSpace::mapMemory(vaddr_t virtualAddress, size_t size,
        int protection) {
    AutoLock lock(&mutex);

    MemorySegment::addSegment(firstSegment, virtualAddress, size, protection);
    paddr_t physicalAddress;

    for (size_t i = 0; i < size; i += 0x1000) {
        physicalAddress = PhysicalMemory::popPageFrame();
        if (!physicalAddress ||
                !mapAt(virtualAddress + i, physicalAddress, protection)) {
            if (physicalAddress) {
                PhysicalMemory::pushPageFrame(physicalAddress);
            }
            for (i = i - 0x1000; i < size; i -= 0x1000) {
                physicalAddress = getPhysicalAddress(virtualAddress + i);
                unmap(virtualAddress + i);
                PhysicalMemory::pushPageFrame(physicalAddress);
            }
            MemorySegment::removeSegment(firstSegment, virtualAddress, size);
            return 0;
        }
    }

    return virtualAddress;
}

vaddr_t AddressSpace::mapPhysical(paddr_t physicalAddress, size_t size,
        int protection) {
    AutoLock lock(&mutex);

    vaddr_t virtualAddress = MemorySegment::findAndAddNewSegment(firstSegment,
            size, protection);
    for (size_t i = 0; i < size; i += 0x1000) {
        if (!mapAt(virtualAddress + i, physicalAddress + i, protection)) {
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

    for (size_t i = 0; i < size; i += 0x1000) {
        paddr_t physicalAddress = getPhysicalAddress(virtualAddress + i);
        unmap(virtualAddress + i);
        PhysicalMemory::pushPageFrame(physicalAddress);
    }

    MemorySegment::removeSegment(firstSegment, virtualAddress, size);
}

void AddressSpace::unmapPhysical(vaddr_t virtualAddress, size_t size) {
    AutoLock lock(&mutex);

    for (size_t i = 0; i < size; i += 0x1000) {
        unmap(virtualAddress + i);
    }

    MemorySegment::removeSegment(firstSegment, virtualAddress, size);
}
