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

#include <stddef.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/kernel.h>

#define RECURSIVE_MAPPING 0xFFC00000

extern "C" {
extern symbol_t bootstrapBegin;
extern symbol_t bootstrapEnd;
}

AddressSpace AddressSpace::_kernelSpace;
AddressSpace* kernelSpace;

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

vaddr_t AddressSpace::mapAt(
        vaddr_t virtualAddress,paddr_t physicalAddress, int flags) {

    size_t pdIndex = virtualAddress >> 22;
    size_t ptIndex = (virtualAddress >> 12) & 0x3FF;

    if (this == kernelSpace) {
        uintptr_t* pageEntry = (uintptr_t*)
                (RECURSIVE_MAPPING + 0x1000 * pdIndex + 4 * ptIndex);
        *pageEntry = physicalAddress | flags;
    } else {
        //TODO: Implement this for other address spaces.
    }

    // Flush the TLB
    asm volatile ("invlpg (%0)" :: "r"(virtualAddress));

    return virtualAddress;
}

void AddressSpace::unmap(vaddr_t virtualAddress) {
    mapAt(virtualAddress, 0, 0);
}
