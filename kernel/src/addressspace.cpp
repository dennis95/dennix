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

#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/kernel.h>
#include <dennix/kernel/log.h>

#define RECURSIVE_MAPPING 0xFFC00000

extern "C" {
extern symbol_t bootstrapBegin;
extern symbol_t bootstrapEnd;
}

AddressSpace AddressSpace::_kernelSpace;
AddressSpace* kernelSpace;

static inline vaddr_t indexToAddress(size_t pdIndex, size_t ptIndex) {
    return (pdIndex << 22) | (ptIndex << 12);
}

static inline void addressToIndex(
        vaddr_t virtualAddress, size_t& pdIndex, size_t& ptIndex) {
    pdIndex = virtualAddress >> 22;
    ptIndex = (virtualAddress >> 12) & 0x3FF;
}

AddressSpace::AddressSpace() {

}

void AddressSpace::initialize() {
    kernelSpace = &_kernelSpace;

    // Unmap the bootstrap sections
    vaddr_t p = (vaddr_t) &bootstrapBegin;

    while (p < (vaddr_t) &bootstrapEnd) {
        kernelSpace->unmap(p);
        p += 0x1000;
    }
}

paddr_t AddressSpace::getPhysicalAddress(vaddr_t virtualAddress) {
    size_t pdIndex;
    size_t ptIndex;
    addressToIndex(virtualAddress, pdIndex, ptIndex);

    if (this == kernelSpace) {
        uintptr_t* pageTable = (uintptr_t*)
                (RECURSIVE_MAPPING + 0x3FF000 + 4 * pdIndex);
        if (*pageTable) {
            uintptr_t* pageEntry = (uintptr_t*)
                    (RECURSIVE_MAPPING + 0x1000 * pdIndex + 4 * ptIndex);
            return *pageEntry & ~0xFFF;
        } else {
            Log::printf("Error: Page Table does not exist.\n");
            return 0;
        }
    } else {
        //TODO: Implement this for other address spaces.
        return 0;
    }

}

vaddr_t AddressSpace::map(paddr_t physicalAddress, int flags) {
    // Find a free page in the higher half and map it
    for (size_t pdIndex = 0x300; pdIndex < 0x400; pdIndex++) {
        for (size_t ptIndex = 0; ptIndex < 0x400; ptIndex++) {
            uintptr_t* pageEntry = (uintptr_t*)
                    (RECURSIVE_MAPPING + 0x1000 * pdIndex + 4 * ptIndex);
            if (!*pageEntry) {
                return mapAt(pdIndex, ptIndex, physicalAddress, flags);
            }
        }
    }

    return 0;
}

vaddr_t AddressSpace::mapAt(
        vaddr_t virtualAddress,paddr_t physicalAddress, int flags) {
    size_t pdIndex;
    size_t ptIndex;
    addressToIndex(virtualAddress, pdIndex, ptIndex);
    return mapAt(pdIndex, ptIndex, physicalAddress, flags);
}

vaddr_t AddressSpace::mapAt(
        size_t pdIndex, size_t ptIndex, paddr_t physicalAddress, int flags) {
    if (this == kernelSpace) {
        uintptr_t* pageTable = (uintptr_t*)
                        (RECURSIVE_MAPPING + 0x3FF000 + 4 * pdIndex);
        if (*pageTable) {
            uintptr_t* pageEntry = (uintptr_t*)
                    (RECURSIVE_MAPPING + 0x1000 * pdIndex + 4 * ptIndex);
            *pageEntry = physicalAddress | flags;
        } else {
            //TODO: Allocate a new page table.
            Log::printf("Error: Page Table does not exist.\n");
            return 0;
        }
    } else {
        //TODO: Implement this for other address spaces.
        return 0;
    }

    vaddr_t virtualAddress = indexToAddress(pdIndex, ptIndex);

    // Flush the TLB
    asm volatile ("invlpg (%0)" :: "r"(virtualAddress));

    return virtualAddress;
}

void AddressSpace::unmap(vaddr_t virtualAddress) {
    mapAt(virtualAddress, 0, 0);
}
