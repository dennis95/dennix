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

/* kernel/include/dennix/kernel/addressspace.h
 * Address space class.
 */

#ifndef KERNEL_ADDRESSSPACE_H
#define KERNEL_ADDRESSSPACE_H

#include <stddef.h>
#include <dennix/kernel/kernel.h>

#define PAGE_PRESENT 0x1
#define PAGE_WRITABLE 0x2

class AddressSpace {
public:
    vaddr_t allocate(size_t nPages);
    void free(vaddr_t virtualAddress, size_t nPages);
    paddr_t getPhysicalAddress(vaddr_t virtualAddress);
    vaddr_t map(paddr_t physicalAddress, int flags);
    vaddr_t mapAt(vaddr_t virtualAddress, paddr_t physicalAddress, int flags);
    vaddr_t mapAt(size_t pdIndex, size_t ptIndex, paddr_t physicalAddress,
            int flags);
    vaddr_t mapRange(paddr_t* physicalAddresses, int flags);
    vaddr_t mapRangeAt(vaddr_t virtualAddress, paddr_t* physicalAddresses,
            int flags);
    void unmap(vaddr_t virtualAddress);
private:
    AddressSpace();
    static AddressSpace _kernelSpace;
public:
    static void initialize();
};

// Global variable for the kernel's address space
extern AddressSpace* kernelSpace;

#endif
