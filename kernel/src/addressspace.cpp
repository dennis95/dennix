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

/* kernel/src/addressspace.cpp
 * Address space class.
 */

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/physicalmemory.h>
#include <dennix/kernel/process.h>
#include <dennix/kernel/syscall.h>

#define RECURSIVE_MAPPING 0xFFC00000

#define PAGE_PRESENT (1 << 0)
#define PAGE_WRITABLE (1 << 1)
#define PAGE_USER (1 << 2)

extern "C" {
extern symbol_t bootstrapBegin;
extern symbol_t bootstrapEnd;
extern symbol_t kernelPageDirectory;
extern symbol_t kernelVirtualBegin;
extern symbol_t kernelReadOnlyEnd;
extern symbol_t kernelVirtualEnd;
}

AddressSpace AddressSpace::_kernelSpace;
AddressSpace* kernelSpace;
static AddressSpace* firstAddressSpace = nullptr;
static kthread_mutex_t listMutex = KTHREAD_MUTEX_INITIALIZER;

static inline vaddr_t indexToAddress(size_t pdIndex, size_t ptIndex) {
    assert(pdIndex <= 0x3FF);
    assert(ptIndex <= 0x3FF);
    return (pdIndex << 22) | (ptIndex << 12);
}

static inline void addressToIndex(
        vaddr_t virtualAddress, size_t& pdIndex, size_t& ptIndex) {
    assert(!(virtualAddress & 0xFFF));
    pdIndex = virtualAddress >> 22;
    ptIndex = (virtualAddress >> 12) & 0x3FF;
}

static inline int protectionToFlags(int protection) {
    int flags = PAGE_PRESENT;
    if (protection & PROT_WRITE) flags |= PAGE_WRITABLE;
    return flags;
}

static vaddr_t mapTemporarily(paddr_t physicalAddress, int protection) {
    return kernelSpace->mapAt(0xFF7FF000, physicalAddress, protection);
}

AddressSpace::AddressSpace() {
    if (this == &_kernelSpace) {
        pageDir = 0;
        pageDirMapped = RECURSIVE_MAPPING + 0x3FF000;
        firstSegment = nullptr;
        prev = nullptr;
        next = nullptr;
    } else {
        pageDir = PhysicalMemory::popPageFrame();

        // Copy the page directory to the new address space
        vaddr_t kernelPageDir = kernelSpace->pageDirMapped;
        pageDirMapped = kernelSpace->mapPhysical(pageDir, 0x1000,
                PROT_READ | PROT_WRITE);

        firstSegment = new MemorySegment(0, 0x1000, PROT_NONE | SEG_NOUNMAP,
                nullptr, nullptr);
        MemorySegment::addSegment(firstSegment, 0xC0000000, -0xC0000000,
                PROT_NONE | SEG_NOUNMAP);

        prev = nullptr;
        AutoLock lock(&listMutex);
        memcpy((void*) pageDirMapped, (const void*) kernelPageDir, 0x1000);
        next = firstAddressSpace;
        if (next) {
            next->prev = this;
        }
        firstAddressSpace = this;
    }

    mutex = KTHREAD_MUTEX_INITIALIZER;
}

AddressSpace::~AddressSpace() {
    kthread_mutex_lock(&listMutex);
    if (prev) {
        prev->next = next;
    }
    if (next) {
        next->prev = prev;
    }
    if (this == firstAddressSpace) {
        firstAddressSpace = next;
    }
    kthread_mutex_unlock(&listMutex);

    MemorySegment* currentSegment = firstSegment;

    while (currentSegment) {
        MemorySegment* next = currentSegment->next;

        if (!(currentSegment->flags & SEG_NOUNMAP)) {
            unmapMemory(currentSegment->address, currentSegment->size);
        }
        currentSegment = next;
    }

    PhysicalMemory::pushPageFrame(pageDir);
}

// We need to create the initial kernel segments at compile time because
// they are needed before memory allocations are possible.
static MemorySegment userSegment(0, 0xC0000000, PROT_NONE, nullptr, nullptr);
static MemorySegment videoSegment(0xC0000000, 0x1000, PROT_READ | PROT_WRITE,
        &userSegment, nullptr);
static MemorySegment readOnlySegment((vaddr_t) &kernelVirtualBegin,
        (vaddr_t) &kernelReadOnlyEnd - (vaddr_t) &kernelVirtualBegin,
        PROT_READ | PROT_EXEC, &videoSegment, nullptr);
static MemorySegment writableSegment((vaddr_t) &kernelReadOnlyEnd,
        (vaddr_t) &kernelVirtualEnd - (vaddr_t) &kernelReadOnlyEnd,
        PROT_READ | PROT_WRITE, &readOnlySegment, nullptr);
static MemorySegment temporarySegment(0xFF7FF000, 0x1000,
        PROT_NONE, &writableSegment, nullptr);
static MemorySegment physicalMemorySegment(RECURSIVE_MAPPING - 0x400000,
        0x400000, PROT_READ | PROT_WRITE, &temporarySegment, nullptr);
static MemorySegment recursiveMappingSegment(RECURSIVE_MAPPING,
        -RECURSIVE_MAPPING, PROT_READ | PROT_WRITE, &physicalMemorySegment,
        nullptr);

void AddressSpace::initialize() {
    kernelSpace = &_kernelSpace;
    kernelSpace->pageDir = (paddr_t) &kernelPageDirectory;

    // Unmap the bootstrap sections
    vaddr_t p = (vaddr_t) &bootstrapBegin;

    while (p < (vaddr_t) &bootstrapEnd) {
        kernelSpace->unmap(p);
        p += 0x1000;
    }

    // Remove the mapping for the bootstrap page table
    // This is the first page table, so know it is mapped at RECURSIVE_MAPPING.
    kernelSpace->unmap(RECURSIVE_MAPPING);

    // Initialize segments for kernel space
    kernelSpace->firstSegment = &userSegment;
    userSegment.next = &videoSegment;
    videoSegment.next = &readOnlySegment;
    readOnlySegment.next = &writableSegment;
    writableSegment.next = &temporarySegment;
    temporarySegment.next = &physicalMemorySegment;
    physicalMemorySegment.next = &recursiveMappingSegment;
}

void AddressSpace::activate() {
    asm volatile ("mov %0, %%cr3" :: "r"(pageDir));
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

paddr_t AddressSpace::getPhysicalAddress(vaddr_t virtualAddress) {
    size_t pdIndex;
    size_t ptIndex;
    addressToIndex(virtualAddress, pdIndex, ptIndex);

    uintptr_t* pageDirectory = (uintptr_t*) pageDirMapped;

    if (!pageDirectory[pdIndex]) {
        return 0;
    }

    uintptr_t* pageTable;
    if (this == kernelSpace) {
        pageTable = (uintptr_t*) (RECURSIVE_MAPPING + 0x1000 * pdIndex);
    } else {
        kthread_mutex_lock(&kernelSpace->mutex);
        pageTable = (uintptr_t*)
                mapTemporarily(pageDirectory[pdIndex] & ~0xFFF, PROT_READ);
    }

    paddr_t result = pageTable[ptIndex] & ~0xFFF;

    if (this != kernelSpace) {
        kernelSpace->unmap((vaddr_t) pageTable);
        kthread_mutex_unlock(&kernelSpace->mutex);
    }

    return result;
}

vaddr_t AddressSpace::mapAt(
        vaddr_t virtualAddress, paddr_t physicalAddress, int protection) {
    size_t pdIndex;
    size_t ptIndex;
    addressToIndex(virtualAddress, pdIndex, ptIndex);
    return mapAt(pdIndex, ptIndex, physicalAddress, protection);
}

vaddr_t AddressSpace::mapAt(size_t pdIndex, size_t ptIndex,
        paddr_t physicalAddress, int protection) {
    assert(!(protection & ~_PROT_FLAGS));
    assert(!(physicalAddress & 0xFFF));

    int flags = protectionToFlags(protection);

    if (this != kernelSpace) {
        // Memory in user space is always accessible by user.
        flags |= PAGE_USER;
    }

    return mapAtWithFlags(pdIndex, ptIndex, physicalAddress, flags);
}

vaddr_t AddressSpace::mapAtWithFlags(size_t pdIndex, size_t ptIndex,
        paddr_t physicalAddress, int flags) {
    assert(!(flags & ~0xFFF));
    assert(!(physicalAddress & 0xFFF));

    uintptr_t* pageDirectory = (uintptr_t*) pageDirMapped;
    uintptr_t* pageTable = nullptr;

    if (this == kernelSpace) {
        pageTable = (uintptr_t*) (RECURSIVE_MAPPING + 0x1000 * pdIndex);
    }

    if (!pageDirectory[pdIndex]) {
        // Allocate a new page table and map it in the page directory
        paddr_t pageTablePhys = PhysicalMemory::popPageFrame();
        int pdFlags = PAGE_PRESENT | PAGE_WRITABLE;
        if (this != kernelSpace) pdFlags |= PAGE_USER;

        pageDirectory[pdIndex] = pageTablePhys | pdFlags;

        if (this != kernelSpace) {
            kthread_mutex_lock(&kernelSpace->mutex);
            pageTable = (uintptr_t*)
                    mapTemporarily(pageTablePhys, PROT_READ | PROT_WRITE);
        }

        memset(pageTable, 0, 0x1000);

        if (this == kernelSpace) {
            // We need to map that page table in all address spaces
            AutoLock lock(&listMutex);

            AddressSpace* addressSpace = firstAddressSpace;
            while (addressSpace) {
                uintptr_t* pd = (uintptr_t*) addressSpace->pageDirMapped;
                pd[pdIndex] = pageTablePhys | PAGE_PRESENT | PAGE_WRITABLE;
                addressSpace = addressSpace->next;
            }
        }

    } else if (this != kernelSpace) {
        kthread_mutex_lock(&kernelSpace->mutex);
        pageTable = (uintptr_t*) mapTemporarily(pageDirectory[pdIndex] &
                ~0xFFF, PROT_READ| PROT_WRITE);
    }

    pageTable[ptIndex] = physicalAddress | flags;

    if (this != kernelSpace) {
        kernelSpace->unmap((vaddr_t) pageTable);
        kthread_mutex_unlock(&kernelSpace->mutex);
    }

    vaddr_t virtualAddress = indexToAddress(pdIndex, ptIndex);

    // Flush the TLB
    asm volatile ("invlpg (%0)" :: "r"(virtualAddress));

    return virtualAddress;
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
    size_t pdIndex, ptIndex;
    addressToIndex(virtualAddress, pdIndex, ptIndex);
    mapAtWithFlags(pdIndex, ptIndex, 0, 0);
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
