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
extern symbol_t kernelVirtualEnd;
}

AddressSpace AddressSpace::_kernelSpace;
AddressSpace* kernelSpace;
static AddressSpace* firstAddressSpace = nullptr;

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

AddressSpace::AddressSpace() {
    if (this == &_kernelSpace) {
        pageDir = 0;
        firstSegment = nullptr;
        next = nullptr;
    } else {
        pageDir = PhysicalMemory::popPageFrame();

        // Copy the page directory to the new address space
        vaddr_t kernelPageDir = (RECURSIVE_MAPPING + 0x3FF000);
        vaddr_t newPageDir = kernelSpace->map(pageDir, PROT_WRITE);
        memcpy((void*) newPageDir, (const void*) kernelPageDir, 0x1000);
        kernelSpace->unmap(newPageDir);

        firstSegment = new MemorySegment(0, 0x1000, PROT_NONE | SEG_NOUNMAP,
                nullptr, nullptr);
        MemorySegment::addSegment(firstSegment, 0xC0000000, -0xC0000000,
                PROT_NONE | SEG_NOUNMAP);

        next = firstAddressSpace;
        firstAddressSpace = this;
    }
}

AddressSpace::~AddressSpace() {
    MemorySegment* currentSegment = firstSegment;

    while (currentSegment) {
        MemorySegment* next = currentSegment->next;

        if (!(currentSegment->flags & SEG_NOUNMAP)) {
            unmapMemory(currentSegment->address, currentSegment->size);
        }
        currentSegment = next;
    }
}

// We need to create the initial kernel segments at compile time because
// they are needed before memory allocations are possible.
static MemorySegment segment1(0, 0xC0000000, PROT_NONE, nullptr, nullptr);
static MemorySegment segment2(0xC0000000, 0x1000, PROT_READ | PROT_WRITE,
        &segment1, nullptr);
static MemorySegment segment3((vaddr_t) &kernelVirtualBegin,
        (vaddr_t) &kernelVirtualEnd - (vaddr_t) &kernelVirtualEnd,
        PROT_READ | PROT_WRITE | PROT_EXEC, &segment2, nullptr);
static MemorySegment segment4(RECURSIVE_MAPPING, -RECURSIVE_MAPPING,
        PROT_READ | PROT_WRITE, &segment3, nullptr);

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
    kernelSpace->firstSegment = &segment1;
    segment1.next = &segment2;
    segment2.next = &segment3;
    segment3.next = &segment4;
}

void AddressSpace::activate() {
    asm volatile ("mov %0, %%cr3" :: "r"(pageDir));
}

AddressSpace* AddressSpace::fork() {
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

    uintptr_t* pageDirectory;
    uintptr_t* pageTable = nullptr;
    paddr_t result = 0;

    if (this == kernelSpace) {
        pageDirectory = (uintptr_t*) (RECURSIVE_MAPPING + 0x3FF000);
        pageTable = (uintptr_t*) (RECURSIVE_MAPPING + 0x1000 * pdIndex);
    } else {
        pageDirectory = (uintptr_t*) kernelSpace->map(pageDir, PROT_READ);
    }

    if (pageDirectory[pdIndex]) {
        if (this != kernelSpace) {
            pageTable = (uintptr_t*) kernelSpace->map(pageDirectory[pdIndex] &
                    ~0xFFF, PROT_READ);
        }
        result = pageTable[ptIndex] & ~0xFFF;
    }

    if (this != kernelSpace) {
        if (pageTable) kernelSpace->unmap((vaddr_t) pageTable);
        kernelSpace->unmap((vaddr_t) pageDirectory);
    }

    return result;
}

bool AddressSpace::isFree(size_t pdIndex, size_t ptIndex) {
    if (pdIndex == 0 && ptIndex == 0) return false;

    uintptr_t* pageDirectory;
    uintptr_t* pageTable = nullptr;
    bool result;

    if (this == kernelSpace) {
        pageDirectory = (uintptr_t*) (RECURSIVE_MAPPING + 0x3FF000);
        pageTable = (uintptr_t*) (RECURSIVE_MAPPING + 0x1000 * pdIndex);
    } else {
        pageDirectory = (uintptr_t*) kernelSpace->map(pageDir, PROT_READ);
    }

    if (!pageDirectory[pdIndex]) {
        result = true;
    } else {
        if (this != kernelSpace) {
            pageTable = (uintptr_t*) kernelSpace->map(
                    pageDirectory[pdIndex] & ~0xFFF, PROT_READ);
        }
        result = !pageTable[ptIndex];
    }

    if (this != kernelSpace) {
        if (pageTable) kernelSpace->unmap((vaddr_t) pageTable);
        kernelSpace->unmap((vaddr_t) pageDirectory);
    }

    return result;
}

vaddr_t AddressSpace::map(paddr_t physicalAddress, int protection) {
    size_t begin;
    size_t end;

    if (this == kernelSpace) {
        begin = 0x300;
        end = 0x400;
    } else {
        begin = 0;
        end = 0x300;
    }

    // Find a free page and map it
    for (size_t pdIndex = begin; pdIndex < end; pdIndex++) {
        for (size_t ptIndex = 0; ptIndex < 0x400; ptIndex++) {
            if (isFree(pdIndex, ptIndex)) {
                return mapAt(pdIndex, ptIndex, physicalAddress, protection);
            }
        }
    }

    return 0;
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
        // Memory in user space is always accessable by user.
        flags |= PAGE_USER;
    }

    return mapAtWithFlags(pdIndex, ptIndex, physicalAddress, flags);
}

vaddr_t AddressSpace::mapAtWithFlags(
        size_t pdIndex, size_t ptIndex, paddr_t physicalAddress, int flags) {
    assert(!(flags & ~0xFFF));
    assert(!(physicalAddress & 0xFFF));

    uintptr_t* pageDirectory;
    uintptr_t* pageTable = nullptr;

    if (this == kernelSpace) {
        pageDirectory = (uintptr_t*) (RECURSIVE_MAPPING + 0x3FF000);
        pageTable = (uintptr_t*) (RECURSIVE_MAPPING + 0x1000 * pdIndex);
    } else {
        pageDirectory = (uintptr_t*) kernelSpace->map(pageDir,
                PROT_READ | PROT_WRITE);
    }

    if (!pageDirectory[pdIndex]) {
        // Allocate a new page table and map it in the page directory
        paddr_t pageTablePhys = PhysicalMemory::popPageFrame();
        int pdFlags = PAGE_PRESENT | PAGE_WRITABLE;
        if (this != kernelSpace) pdFlags |= PAGE_USER;

        pageDirectory[pdIndex] = pageTablePhys | pdFlags;

        if (this != kernelSpace) {
            pageTable = (uintptr_t*) kernelSpace->map(pageTablePhys,
                    PROT_READ | PROT_WRITE);
        }

        memset(pageTable, 0, 0x1000);

        if (this == kernelSpace) {
            // We need to map that page table in all address spaces
            AddressSpace* addressSpace = firstAddressSpace;
            while (addressSpace) {
                uintptr_t* pageDir = (uintptr_t*) map(addressSpace->pageDir,
                        PROT_READ | PROT_WRITE);
                pageDir[pdIndex] = pageTablePhys |
                        PAGE_PRESENT | PAGE_WRITABLE;
                unmap((vaddr_t) pageDir);
                addressSpace = addressSpace->next;
            }
        }

    } else if (this != kernelSpace) {
        pageTable = (uintptr_t*) kernelSpace->map(
                pageDirectory[pdIndex] & ~0xFFF, PROT_READ| PROT_WRITE);
    }

    pageTable[ptIndex] = physicalAddress | flags;

    if (this != kernelSpace) {
        kernelSpace->unmap((vaddr_t) pageTable);
        kernelSpace->unmap((vaddr_t) pageDirectory);
    }

    vaddr_t virtualAddress = indexToAddress(pdIndex, ptIndex);

    // Flush the TLB
    asm volatile ("invlpg (%0)" :: "r"(virtualAddress));

    return virtualAddress;
}

vaddr_t AddressSpace::mapFromOtherAddressSpace(AddressSpace* sourceSpace,
        vaddr_t sourceVirtualAddress, size_t size, int protection) {
    vaddr_t destination = MemorySegment::findFreeSegment(firstSegment, size);

    for (size_t i = 0 ; i < size; i += 0x1000) {
        paddr_t physicalAddress =
                sourceSpace->getPhysicalAddress(sourceVirtualAddress + i);
        if (!physicalAddress ||
                !mapAt(destination + i, physicalAddress, protection)) {
            return 0;
        }
    }

    MemorySegment::addSegment(firstSegment, destination, size, protection);

    return destination;
}

vaddr_t AddressSpace::mapMemory(size_t size, int protection) {
    vaddr_t virtualAddress =
            MemorySegment::findFreeSegment(firstSegment, size);
    return mapMemory(virtualAddress, size, protection);
}

vaddr_t AddressSpace::mapMemory(vaddr_t virtualAddress, size_t size,
        int protection) {
    paddr_t physicalAddress;

    for (size_t i = 0; i < size; i += 0x1000) {
        physicalAddress = PhysicalMemory::popPageFrame();
        if (!physicalAddress ||
                !mapAt(virtualAddress + i, physicalAddress, protection)) {
            return 0;
        }
    }

    MemorySegment::addSegment(firstSegment, virtualAddress, size, protection);

    return virtualAddress;
}

vaddr_t AddressSpace::mapPhysical(paddr_t physicalAddress, size_t size,
        int protection) {
    vaddr_t virtualAddress =
            MemorySegment::findFreeSegment(firstSegment, size);
    return mapPhysical(virtualAddress, physicalAddress, size, protection);
}

vaddr_t AddressSpace::mapPhysical(vaddr_t virtualAddress,
        paddr_t physicalAddress, size_t size, int protection) {
    for (size_t i = 0; i < size; i += 0x1000) {
        if (!mapAt(virtualAddress + i, physicalAddress + i, protection)) {
            return 0;
        }
    }

    MemorySegment::addSegment(firstSegment, virtualAddress, size, protection);

    return virtualAddress;
}

void AddressSpace::unmap(vaddr_t virtualAddress) {
    size_t pdIndex, ptIndex;
    addressToIndex(virtualAddress, pdIndex, ptIndex);
    mapAtWithFlags(pdIndex, ptIndex, 0, 0);
}

void AddressSpace::unmapMemory(vaddr_t virtualAddress, size_t size) {
    for (size_t i = 0; i < size; i += 0x1000) {
        paddr_t physicalAddress = getPhysicalAddress(virtualAddress + i);
        unmap(virtualAddress + i);
        PhysicalMemory::pushPageFrame(physicalAddress);
    }

    MemorySegment::removeSegment(firstSegment, virtualAddress, size);
}

void AddressSpace::unmapPhysical(vaddr_t virtualAddress, size_t size) {
    for (size_t i = 0; i < size; i += 0x1000) {
        unmap(virtualAddress + i);
    }

    MemorySegment::removeSegment(firstSegment, virtualAddress, size);
}

static void* mmapImplementation(void* /*addr*/, size_t size,
        int protection, int flags, int /*fd*/, off_t /*offset*/) {
    if (size == 0 || !(flags & MAP_PRIVATE)) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    if (flags & MAP_ANONYMOUS) {
        AddressSpace* addressSpace = Process::current->addressSpace;
        return (void*) addressSpace->mapMemory(size, protection);
    }

    //TODO: Implement other flags than MAP_ANONYMOUS
    errno = ENOTSUP;
    return MAP_FAILED;
}

void* Syscall::mmap(__mmapRequest* request) {
    return mmapImplementation(request->_addr, request->_size,
            request->_protection, request->_flags, request->_fd,
            request->_offset);
}

int Syscall::munmap(void* addr, size_t size) {
    if (size == 0 || ((vaddr_t) addr & 0xFFF)) {
        errno = EINVAL;
        return -1;
    }

    AddressSpace* addressSpace = Process::current->addressSpace;
    //TODO: The userspace process could unmap kernel pages!
    addressSpace->unmapMemory((vaddr_t) addr, size);
    return 0;
}

// These two functions are called from libk.
extern "C" void* __mapMemory(size_t size) {
    return (void*) kernelSpace->mapMemory(size, PROT_READ | PROT_WRITE);
}

extern "C" void __unmapMemory(void* addr, size_t size) {
    kernelSpace->unmapMemory((vaddr_t) addr, size);
}
