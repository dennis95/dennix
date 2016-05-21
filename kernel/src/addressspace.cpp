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
#include <string.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/kernel.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/physicalmemory.h>

#define RECURSIVE_MAPPING 0xFFC00000

extern "C" {
extern symbol_t bootstrapBegin;
extern symbol_t bootstrapEnd;
extern symbol_t kernelPageDirectory;
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

AddressSpace::AddressSpace() {
    pageDir = 0;
    next = nullptr;
}

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
}

void AddressSpace::activate() {
    asm volatile ("mov %0, %%cr3" :: "r"(pageDir));
}

vaddr_t AddressSpace::allocate(size_t nPages) {
    paddr_t physicalAddresses[nPages + 1];

    for (size_t i = 0; i < nPages; i++) {
        physicalAddresses[i] = PhysicalMemory::popPageFrame();
        if (!physicalAddresses[i]) return 0;
    }
    physicalAddresses[nPages] = 0;

    int flags = PAGE_PRESENT | PAGE_WRITABLE;
    if (this != kernelSpace) flags |= PAGE_USER;

    return mapRange(physicalAddresses, flags);
}

AddressSpace* AddressSpace::fork() {
    AddressSpace* result = new AddressSpace();
    result->pageDir = PhysicalMemory::popPageFrame();

    // Map the new and the old page directories so we can copy them
    vaddr_t currentPageDir = kernelSpace->map(pageDir, PAGE_PRESENT);
    vaddr_t newPageDir = kernelSpace->map(result->pageDir,
            PAGE_PRESENT | PAGE_WRITABLE);

    memcpy((void*) newPageDir, (const void*) currentPageDir, 0x1000);

    kernelSpace->unmap(currentPageDir);
    kernelSpace->unmap(newPageDir);

    result->next = firstAddressSpace;
    firstAddressSpace = result;

    return result;
}

void AddressSpace::free(vaddr_t virtualAddress, size_t nPages) {
    for (size_t i = 0; i < nPages; i++) {
        paddr_t physicalAddress = getPhysicalAddress(virtualAddress);
        unmap(virtualAddress);
        PhysicalMemory::pushPageFrame(physicalAddress);
        virtualAddress += 0x1000;
    }
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
        pageDirectory = (uintptr_t*) kernelSpace->map(pageDir, PAGE_PRESENT);
    }

    if (pageDirectory[pdIndex]) {
        if (this != kernelSpace) {
            pageTable = (uintptr_t*) kernelSpace->map(pageDirectory[pdIndex] &
                    ~0xFFF,PAGE_PRESENT);
        }
        result = pageTable[ptIndex] & ~0xFFF;
    }

    if (this != kernelSpace) {
        if (pageTable) kernelSpace->unmap((vaddr_t) pageTable);
        kernelSpace->unmap((vaddr_t) pageDirectory);
    }

    return result;
}

bool AddressSpace::isFree(vaddr_t virtualAddress) {
    size_t pdIndex;
    size_t ptIndex;
    addressToIndex(virtualAddress, pdIndex, ptIndex);
    return isFree(pdIndex, ptIndex);
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
        pageDirectory = (uintptr_t*) kernelSpace->map(pageDir, PAGE_PRESENT);
    }

    if (!pageDirectory[pdIndex]) {
        result = true;
    } else {
        if (this != kernelSpace) {
            pageTable = (uintptr_t*) kernelSpace->map(
                    pageDirectory[pdIndex] & ~0xFFF, PAGE_PRESENT);
        }
        result = !pageTable[ptIndex];
    }

    if (this != kernelSpace) {
        if (pageTable) kernelSpace->unmap((vaddr_t) pageTable);
        kernelSpace->unmap((vaddr_t) pageDirectory);
    }

    return result;
}

vaddr_t AddressSpace::map(paddr_t physicalAddress, int flags) {
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
                return mapAt(pdIndex, ptIndex, physicalAddress, flags);
            }
        }
    }

    return 0;
}

vaddr_t AddressSpace::mapAt(
        vaddr_t virtualAddress, paddr_t physicalAddress, int flags) {
    size_t pdIndex;
    size_t ptIndex;
    addressToIndex(virtualAddress, pdIndex, ptIndex);
    return mapAt(pdIndex, ptIndex, physicalAddress, flags);
}

vaddr_t AddressSpace::mapAt(
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
                PAGE_PRESENT | PAGE_WRITABLE);
    }

    if (!pageDirectory[pdIndex]) {
        // Allocate a new page table and map it in the page directory
        paddr_t pageTablePhys = PhysicalMemory::popPageFrame();
        int pdFlags = PAGE_PRESENT | PAGE_WRITABLE;
        if (this != kernelSpace) pdFlags |= PAGE_USER;

        pageDirectory[pdIndex] = pageTablePhys | pdFlags;

        if (this != kernelSpace) {
            pageTable = (uintptr_t*) kernelSpace->map(pageTablePhys,
                    PAGE_PRESENT | PAGE_WRITABLE);
        }

        memset(pageTable, 0, 0x1000);

        if (this == kernelSpace) {
            // We need to map that page table in all address spaces
            AddressSpace* addressSpace = firstAddressSpace;
            while (addressSpace) {
                uintptr_t* pageDir = (uintptr_t*) map(addressSpace->pageDir,
                        PAGE_PRESENT | PAGE_WRITABLE);
                pageDir[pdIndex] = pageTablePhys |
                        PAGE_PRESENT | PAGE_WRITABLE;
                unmap((vaddr_t) pageDir);
                addressSpace = addressSpace->next;
            }
        }

    } else if (this != kernelSpace) {
        pageTable = (uintptr_t*) kernelSpace->map(
                pageDirectory[pdIndex] & ~0xFFF, PAGE_PRESENT | PAGE_WRITABLE);
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

vaddr_t AddressSpace::mapRange(paddr_t* physicalAddresses, int flags) {
    paddr_t* phys = physicalAddresses;
    size_t nPages = 0;

    while (*phys++){
        nPages++;
    }

    size_t begin;
    size_t end;

    if (this == kernelSpace) {
        begin = 0x300;
        end = 0x400;
    } else {
        begin = 0;
        end = 0x300;
    }

    // Find enough free pages in the higher half and map them
    for (size_t pdIndex = begin; pdIndex < end; pdIndex++) {
        for (size_t ptIndex = 0; ptIndex < 0x400; ptIndex++) {
            if (pdIndex == 0 && ptIndex == 0) continue;
            size_t pd = pdIndex;
            size_t pt = ptIndex;
            size_t foundPages = 0;

            while (foundPages <= nPages) {
                if (!isFree(pd, pt)) break;

                pt++;
                foundPages++;

                if (pt >= 1024) {
                    pd++;
                    pt = 0;
                }
            }

            if (foundPages >= nPages) {
                return mapRangeAt(indexToAddress(pdIndex, ptIndex),
                        physicalAddresses, flags);
            }
        }
    }

    return 0;
}

vaddr_t AddressSpace::mapRange(paddr_t firstPhysicalAddress,
        size_t nPages, int flags) {
    paddr_t physicalAddresses[nPages + 1];

    for (size_t i = 0; i < nPages; i++) {
        physicalAddresses[i] = firstPhysicalAddress;
        firstPhysicalAddress += 0x1000;
    }
    physicalAddresses[nPages] = 0;

    return mapRange(physicalAddresses, flags);
}

vaddr_t AddressSpace::mapRangeAt(vaddr_t virtualAddress,
        paddr_t* physicalAddresses, int flags) {
    vaddr_t addr = virtualAddress;

    while (*physicalAddresses) {
        if (!mapAt(addr, *physicalAddresses, flags)) {
            return 0;
        }
        addr += 0x1000;
        physicalAddresses++;
    }

    return virtualAddress;
}

void AddressSpace::unmap(vaddr_t virtualAddress) {
    mapAt(virtualAddress, 0, 0);
}

void AddressSpace::unmapRange(vaddr_t firstVirtualAddress, size_t nPages) {
    while (nPages--) {
        unmap(firstVirtualAddress);
        firstVirtualAddress += 0x1000;
    }
}

// These two functions are called from libk.
extern "C" void* __mapPages(size_t nPages) {
    return (void*) kernelSpace->allocate(nPages);
}

extern "C" void __unmapPages(void* addr, size_t nPages) {
    kernelSpace->free((vaddr_t) addr, nPages);
}
