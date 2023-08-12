/* Copyright (c) 2019, 2020, 2021, 2023 Dennis Wölfing
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

/* kernel/src/arch/x86_64/addressspace.cpp
 * Address space class.
 */

#include <assert.h>
#include <string.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/physicalmemory.h>

#define RECURSIVE_MAPPING 0xFFFFFF0000000000

#define INDEX2ADDR(pml4, pdpt, pd) \
        (RECURSIVE_MAPPING | (pml4) << 30 | (pdpt) << 21 | (pd) << 12)
#define RECURSIVE_PAGETABLE(pml4, pdpt, pd) INDEX2ADDR(pml4, pdpt, pd)
#define RECURSIVE_PAGEDIR(pml4, pdpt) INDEX2ADDR(510UL, pml4, pdpt)
#define RECURSIVE_PDPT(pml4) INDEX2ADDR(510UL, 510UL, pml4)
#define RECURSIVE_PML4() INDEX2ADDR(510UL, 510UL, 510UL)

#define PAGE_PRESENT (1 << 0)
#define PAGE_WRITABLE (1 << 1)
#define PAGE_USER (1 << 2)
#define PAGE_WRITE_COMBINING (1 << 7)
#define PAGE_NO_EXECUTE (1UL << 63)
#define PAGE_FLAGS 0xFFF0000000000FFF

extern "C" {
extern symbol_t bootstrapBegin;
extern symbol_t bootstrapEnd;
extern symbol_t kernelPml4;
extern symbol_t kernelVirtualBegin;
extern symbol_t kernelExecEnd;
extern symbol_t kernelReadOnlyEnd;
extern symbol_t kernelVirtualEnd;
}

static kthread_mutex_t listMutex = KTHREAD_MUTEX_INITIALIZER;
static char _kernelMappingArea[PAGESIZE] ALIGNED(PAGESIZE);

// We need to create the initial kernel segments at compile time because
// they are needed before memory allocations are possible.
static MemorySegment kernelSegments[] = {
    MemorySegment(0, 0xFFFF800000000000, PROT_NONE),
    MemorySegment(RECURSIVE_MAPPING, 0x8000000000, PROT_READ | PROT_WRITE),
    MemorySegment((vaddr_t) &kernelVirtualBegin, (vaddr_t) &kernelExecEnd -
            (vaddr_t) &kernelVirtualBegin, PROT_EXEC),
    MemorySegment((vaddr_t) &kernelExecEnd, (vaddr_t) &kernelReadOnlyEnd -
            (vaddr_t) &kernelExecEnd, PROT_READ),
    MemorySegment((vaddr_t) &kernelReadOnlyEnd, (vaddr_t) &kernelVirtualEnd -
            (vaddr_t) &kernelReadOnlyEnd, PROT_READ | PROT_WRITE),
};

struct PageIndex {
    size_t pml4Index;
    size_t pdptIndex;
    size_t pdIndex;
    size_t ptIndex;
};

static PageIndex addressToIndex(vaddr_t virtualAddress) {
    assert(PAGE_ALIGNED(virtualAddress));
    assert(virtualAddress <= 0x7FFFFFFFF000 ||
            virtualAddress >= 0xFFFF800000000000);

    PageIndex result;
    result.pml4Index = (virtualAddress >> 39) & 0x1FF;
    result.pdptIndex = (virtualAddress >> 30) & 0x1FF;
    result.pdIndex = (virtualAddress >> 21) & 0x1FF;
    result.ptIndex = (virtualAddress >> 12) & 0x1FF;
    return result;
}

static inline uintptr_t protectionToFlags(int protection) {
    uintptr_t flags = PAGE_PRESENT;
    if (protection & PROT_WRITE) flags |= PAGE_WRITABLE;
    if (!(protection & PROT_EXEC)) flags |= PAGE_NO_EXECUTE;
    if (protection & PROT_WRITE_COMBINING && AddressSpace::patSupported) {
        flags |= PAGE_WRITE_COMBINING;
    }
    return flags;
}

AddressSpace::AddressSpace() {
    if (this == kernelSpace) {
        pml4 = (paddr_t) &kernelPml4;
        mappingArea = (vaddr_t) _kernelMappingArea;
        prev = nullptr;
        next = nullptr;
    } else {
        pml4 = PhysicalMemory::popPageFrame();
        if (!pml4) FAIL_CONSTRUCTOR;

        if (!MemorySegment::addSegment(segments, 0, PAGESIZE,
                PROT_NONE | SEG_NOUNMAP) || !MemorySegment::addSegment(segments,
                0x800000000000, -0x800000000000, PROT_NONE | SEG_NOUNMAP)) {
            FAIL_CONSTRUCTOR;
        }

        mappingArea = MemorySegment::findAndAddNewSegment(
                kernelSpace->segments, PAGESIZE, PROT_NONE);
        if (!mappingArea) FAIL_CONSTRUCTOR;

        AutoLock lock(&listMutex);
        next = kernelSpace->next;
        if (next) {
            next->prev = this;
        }
        prev = kernelSpace;
        kernelSpace->next = this;

        // Copy the kernel page directory into the new address space.
        kernelSpace->mapAt(kernelSpace->mappingArea, pml4, PROT_WRITE);
        memset((void*) kernelSpace->mappingArea, 0, 0x800);
        memcpy((void*) (kernelSpace->mappingArea + 0x800),
                (const void*) (RECURSIVE_PML4() + 0x800), 0x800);
        *((paddr_t*) (kernelSpace->mappingArea + 0xFF0)) = pml4 |
                PAGE_PRESENT | PAGE_WRITABLE;
        kernelSpace->unmap(kernelSpace->mappingArea);
    }

    mutex = KTHREAD_MUTEX_INITIALIZER;
}

AddressSpace::~AddressSpace() {
    if (!pml4) return;
    if (!__constructionFailed) {
        kthread_mutex_lock(&listMutex);
        prev->next = next;
        if (next) {
            next->prev = prev;
        }
        kthread_mutex_unlock(&listMutex);
    }

    auto currentSegment = segments.begin();
    while (currentSegment != segments.end()) {
        auto next = Util::next(currentSegment);
        if (!(currentSegment->flags & SEG_NOUNMAP)) {
            unmapMemory(currentSegment->address, currentSegment->size);
        }
        currentSegment = next;
    }

    if (!__constructionFailed) {
        // Free the PDPTs, page directories and page tables.
        uintptr_t* mapped = (uintptr_t*) kernelSpace->mapAt(mappingArea,
                pml4, PROT_READ);
        for (size_t i = 0; i < 256; i++) {
            paddr_t pdpt = mapped[i] & ~PAGE_MISALIGN;
            if (pdpt) {
                kernelSpace->mapAt(mappingArea, pdpt, PROT_READ);
                for (size_t j = 0; j < 512; j++) {
                    paddr_t pd = mapped[j] & ~PAGE_MISALIGN;
                    if (pd) {
                        kernelSpace->mapAt(mappingArea, pd, PROT_READ);
                        for (size_t k = 0; k < 512; k++) {
                            paddr_t pt = mapped[k] & ~PAGE_MISALIGN;
                            if (pt) {
                                PhysicalMemory::pushPageFrame(pt);
                            }
                        }
                        PhysicalMemory::pushPageFrame(pd);
                        kernelSpace->mapAt(mappingArea, pdpt, PROT_READ);
                    }
                }
                PhysicalMemory::pushPageFrame(pdpt);
                kernelSpace->mapAt(mappingArea, pml4, PROT_READ);
            }
        }
        kernelSpace->unmap(mappingArea);
        MemorySegment::removeSegment(kernelSpace->segments, mappingArea,
                PAGESIZE);
    }
    while (!segments.empty()) {
        MemorySegment& segment = segments.front();
        segments.remove(segment);
        MemorySegment::deallocateSegment(&segment);
    }
    PhysicalMemory::pushPageFrame(pml4);
}

void AddressSpace::initialize() {
    kernelSpace->segments.addFront(kernelSegments[4]);
    kernelSpace->segments.addFront(kernelSegments[3]);
    kernelSpace->segments.addFront(kernelSegments[2]);
    kernelSpace->segments.addFront(kernelSegments[1]);
    kernelSpace->segments.addFront(kernelSegments[0]);

    // Unmap the bootstrap sections
    vaddr_t p = (vaddr_t) &bootstrapBegin;

    while (p < (vaddr_t) &bootstrapEnd) {
        kernelSpace->unmap(p);
        if (p < (paddr_t) &kernelPml4) {
            PhysicalMemory::pushPageFrame(p);
        }
        p += PAGESIZE;
    }

    // Remove the mapping for the bootstrap paging structures.
    kernelSpace->unmap(RECURSIVE_PAGETABLE(0, 0, 0));
    kernelSpace->unmap(RECURSIVE_PAGEDIR(0, 0));
    kernelSpace->unmap(RECURSIVE_PDPT(0));

    uint32_t eax = 1;
    uint32_t edx;
    asm("cpuid" : "+a"(eax), "=d"(edx) :: "ebx", "ecx");
    patSupported = edx & (1 << 16);
    if (patSupported) {
        uint32_t patLow;
        uint32_t patHigh;
        asm("rdmsr" : "=a"(patLow), "=d"(patHigh) : "c"(0x277));
        // Set PAT entry 4 to write combining.
        patHigh = (patHigh & 0xFFFFFF00) | 0x01;
        asm("wrmsr" :: "a"(patLow), "d"(patHigh), "c"(0x277));
    }
}

void AddressSpace::activate() {
    activeAddressSpace = this;
    asm ("mov %0, %%cr3" :: "r"(pml4));
}

paddr_t AddressSpace::getPhysicalAddress(vaddr_t virtualAddress) {
    if (this == kernelSpace && virtualAddress < 0xFFFF800000000000) return 0;
    PageIndex index = addressToIndex(virtualAddress);

    if (isActive()) {
        uintptr_t* pml4 = (uintptr_t*) RECURSIVE_PML4();
        if (!pml4[index.pml4Index]) return 0;
        uintptr_t* pdpt = (uintptr_t*) RECURSIVE_PDPT(index.pml4Index);
        if (!pdpt[index.pdptIndex]) return 0;
        uintptr_t* pageDir = (uintptr_t*) RECURSIVE_PAGEDIR(index.pml4Index,
                index.pdptIndex);
        if (!pageDir[index.pdIndex]) return 0;
        uintptr_t* pageTable = (uintptr_t*) RECURSIVE_PAGETABLE(index.pml4Index,
                index.pdptIndex, index.pdIndex);
        return pageTable[index.ptIndex] & ~PAGE_FLAGS;
    } else {
        uintptr_t* pml4Mapping = (uintptr_t*) kernelSpace->mapAt(mappingArea,
                pml4, PROT_READ);
        uintptr_t pml4Entry = pml4Mapping[index.pml4Index];
        kernelSpace->unmap(mappingArea);
        if (!pml4Entry) return 0;

        uintptr_t* pdpt = (uintptr_t*) kernelSpace->mapAt(mappingArea,
                pml4Entry & ~PAGE_FLAGS, PROT_READ);
        uintptr_t pdptEntry = pdpt[index.pdptIndex];
        kernelSpace->unmap(mappingArea);
        if (!pdptEntry) return 0;

        uintptr_t* pageDirectory = (uintptr_t*) kernelSpace->mapAt(mappingArea,
                pdptEntry & ~PAGE_FLAGS, PROT_READ);
        uintptr_t pdEntry = pageDirectory[index.pdIndex];
        kernelSpace->unmap(mappingArea);
        if (!pdEntry) return 0;

        uintptr_t* pageTable = (uintptr_t*) kernelSpace->mapAt(mappingArea,
                pdEntry & ~PAGE_FLAGS, PROT_READ);
        paddr_t result = pageTable[index.ptIndex] & ~PAGE_FLAGS;
        kernelSpace->unmap(mappingArea);
        return result;
    }
}

vaddr_t AddressSpace::mapAt(vaddr_t virtualAddress, paddr_t physicalAddress,
        int protection) {
    assert(!(physicalAddress & PAGE_FLAGS));

    uintptr_t flags = protectionToFlags(protection);

    if (this != kernelSpace) {
        // Memory in user space is always accessible by user.
        flags |= PAGE_USER;
    }
    if (!physicalAddress) {
        flags = 0;
    }

    PageIndex index = addressToIndex(virtualAddress);
    uintptr_t* pml4Mapping = (uintptr_t*) RECURSIVE_PML4();
    uintptr_t* pdpt = (uintptr_t*) RECURSIVE_PDPT(index.pml4Index);
    uintptr_t* pageDir = (uintptr_t*) RECURSIVE_PAGEDIR(index.pml4Index,
            index.pdptIndex);
    uintptr_t* pageTable = (uintptr_t*) RECURSIVE_PAGETABLE(index.pml4Index,
            index.pdptIndex, index.pdIndex);

    if (!isActive()) {
        pml4Mapping = (uintptr_t*) kernelSpace->mapAt(mappingArea, pml4,
                PROT_READ | PROT_WRITE);
    }

    if (!pml4Mapping[index.pml4Index]) {
        paddr_t pdptPhys = PhysicalMemory::popPageFrame();
        if (!pdptPhys) goto fail;
        uintptr_t pdptFlags = PAGE_PRESENT | PAGE_WRITABLE;
        if (this != kernelSpace) pdptFlags |= PAGE_USER;

        if (this == kernelSpace) {
            // We need to map that pdpt in all address spaces
            AutoLock lock(&listMutex);

            AddressSpace* addressSpace = kernelSpace;
            while (addressSpace) {
                uintptr_t* pml4Mapped = (uintptr_t*)
                        kernelSpace->mapAt(kernelSpace->mappingArea,
                        addressSpace->pml4, PROT_WRITE);
                pml4Mapped[index.pml4Index] = pdptPhys | pdptFlags;
                kernelSpace->unmap(kernelSpace->mappingArea);
                addressSpace = addressSpace->next;
            }
        } else if (isActive()) {
            pml4Mapping[index.pml4Index] = pdptPhys | pdptFlags;
            asm ("invlpg (%0)" :: "r"(RECURSIVE_PDPT(index.pml4Index)));
        } else {
            pml4Mapping[index.pml4Index] = pdptPhys | pdptFlags;
            kernelSpace->unmap(mappingArea);
            pdpt = (uintptr_t*) kernelSpace->mapAt(mappingArea, pdptPhys,
                    PROT_READ | PROT_WRITE);
        }

        memset(pdpt, 0, PAGESIZE);
    } else if (!isActive()) {
        paddr_t pdptPhys = pml4Mapping[index.pml4Index] & ~PAGE_FLAGS;
        kernelSpace->unmap(mappingArea);
        pdpt = (uintptr_t*) kernelSpace->mapAt(mappingArea, pdptPhys,
                PROT_READ | PROT_WRITE);
    }

    if (!pdpt[index.pdptIndex]) {
        paddr_t pdPhys = PhysicalMemory::popPageFrame();
        if (!pdPhys) goto fail;
        uintptr_t pdFlags = PAGE_PRESENT | PAGE_WRITABLE;
        if (this != kernelSpace) pdFlags |= PAGE_USER;

        pdpt[index.pdptIndex] = pdPhys | pdFlags;
        if (isActive()) {
            asm ("invlpg (%0)" :: "r"(RECURSIVE_PAGEDIR(index.pml4Index,
                    index.pdptIndex)));
        } else {
            kernelSpace->unmap(mappingArea);
            pageDir = (uintptr_t*) kernelSpace->mapAt(mappingArea, pdPhys,
                    PROT_READ | PROT_WRITE);
        }
        memset(pageDir, 0, PAGESIZE);
    } else if (!isActive()) {
        paddr_t pdPhys = pdpt[index.pdptIndex] & ~PAGE_FLAGS;
        kernelSpace->unmap(mappingArea);
        pageDir = (uintptr_t*) kernelSpace->mapAt(mappingArea, pdPhys,
                PROT_READ | PROT_WRITE);
    }

    if (!pageDir[index.pdIndex]) {
        paddr_t ptPhys = PhysicalMemory::popPageFrame();
        if (!ptPhys) goto fail;
        uintptr_t ptFlags = PAGE_PRESENT | PAGE_WRITABLE;
        if (this != kernelSpace) ptFlags |= PAGE_USER;

        pageDir[index.pdIndex] = ptPhys | ptFlags;
        if (isActive()) {
            asm ("invlpg (%0)" :: "r"(RECURSIVE_PAGETABLE(index.pml4Index,
                    index.pdptIndex, index.pdIndex)));
        } else {
            kernelSpace->unmap(mappingArea);
            pageTable = (uintptr_t*) kernelSpace->mapAt(mappingArea, ptPhys,
                    PROT_READ | PROT_WRITE);
        }
        memset(pageTable, 0, PAGESIZE);
    } else if (!isActive()) {
        paddr_t ptPhys = pageDir[index.pdIndex] & ~PAGE_FLAGS;
        kernelSpace->unmap(mappingArea);
        pageTable = (uintptr_t*) kernelSpace->mapAt(mappingArea, ptPhys,
                PROT_READ | PROT_WRITE);
    }

    pageTable[index.ptIndex] = physicalAddress | flags;

    if (isActive()) {
        asm ("invlpg (%0)" :: "r"(virtualAddress));
    } else {
        kernelSpace->unmap(mappingArea);
    }

    return virtualAddress;

fail:
    if (!isActive()) {
        kernelSpace->unmap(mappingArea);
    }
    return 0;
}
