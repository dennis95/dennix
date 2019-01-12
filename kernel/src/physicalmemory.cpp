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

/* kernel/src/physicalmemory.cpp
 * Physical memory management.
 */

#include <assert.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/kthread.h>
#include <dennix/kernel/physicalmemory.h>

static char firstStackPage[0x1000] ALIGNED(0x1000);
static paddr_t* stack = (paddr_t*) firstStackPage + 1;
static vaddr_t lastStackPage = (vaddr_t) firstStackPage;

static kthread_mutex_t mutex = KTHREAD_MUTEX_INITIALIZER;

extern "C" {
extern symbol_t bootstrapBegin;
extern symbol_t bootstrapEnd;
extern symbol_t kernelPhysicalBegin;
extern symbol_t kernelPhysicalEnd;
}

static inline bool isUsedByKernel(paddr_t physicalAddress) {
    return (physicalAddress >= (paddr_t) &bootstrapBegin &&
            physicalAddress < (paddr_t) &bootstrapEnd) ||
            (physicalAddress >= (paddr_t) &kernelPhysicalBegin &&
            physicalAddress < (paddr_t) &kernelPhysicalEnd) ||
            physicalAddress == 0;
}

static inline bool isUsedByModule(paddr_t physicalAddress,
        multiboot_mod_list* modules, uint32_t moduleCount) {
    for (size_t i = 0; i < moduleCount; i++) {
        if (physicalAddress >= modules[i].mod_start &&
                physicalAddress < modules[i].mod_end) {
            return true;
        }
    }
    return false;
}

static inline bool isUsedByMultiboot(paddr_t physicalAddress,
        multiboot_info* multiboot) {
    paddr_t mmapEnd = multiboot->mmap_addr + multiboot->mmap_length;
    paddr_t modsEnd = multiboot->mods_addr +
            multiboot->mods_count * sizeof(multiboot_mod_list);
    return ((physicalAddress >= (multiboot->mmap_addr & ~0xFFF) &&
            physicalAddress < mmapEnd) ||
            (physicalAddress >= (multiboot->mods_addr & ~0xFFF) &&
            physicalAddress < modsEnd));
}

void PhysicalMemory::initialize(multiboot_info* multiboot) {
    paddr_t mmapPhys = (paddr_t) multiboot->mmap_addr;
    paddr_t mmapAligned = mmapPhys & ~0xFFF;
    ptrdiff_t mmapOffset = mmapPhys - mmapAligned;
    size_t mmapSize = ALIGNUP(mmapOffset + multiboot->mmap_length, 0x1000);

    // Map the module list so we can check which addresses are used by modules
    paddr_t modulesPhys = (paddr_t) multiboot->mods_addr;
    paddr_t modulesAligned = modulesPhys & ~0xFFF;
    ptrdiff_t modulesOffset = modulesPhys - modulesAligned;
    size_t modulesSize = ALIGNUP(modulesOffset +
            multiboot->mods_count * sizeof(multiboot_mod_list), 0x1000);

    vaddr_t mmapMapped = kernelSpace->mapPhysical(mmapAligned, mmapSize,
            PROT_READ);
    vaddr_t modulesMapped = kernelSpace->mapPhysical(modulesAligned,
            modulesSize, PROT_READ);

    vaddr_t mmap = mmapMapped + mmapOffset;
    vaddr_t mmapEnd = mmap + multiboot->mmap_length;

    multiboot_mod_list* modules =
            (multiboot_mod_list*) (modulesMapped + modulesOffset);

    while (mmap < mmapEnd) {
        multiboot_mmap_entry* mmapEntry = (multiboot_mmap_entry*) mmap;

        if (mmapEntry->type == MULTIBOOT_MEMORY_AVAILABLE &&
            mmapEntry->addr + mmapEntry->len <= UINTPTR_MAX) {
            paddr_t addr = (paddr_t) mmapEntry->addr;
            for (uint64_t i = 0; i < mmapEntry->len; i += 0x1000) {
                if (isUsedByModule(addr + i, modules, multiboot->mods_count) ||
                        isUsedByKernel(addr + i) ||
                        isUsedByMultiboot(addr + i, multiboot)) {
                    continue;
                }

                pushPageFrame(addr + i);
            }
        }

        mmap += mmapEntry->size + 4;
    }

    kernelSpace->unmapPhysical(mmapMapped, mmapSize);
    kernelSpace->unmapPhysical(modulesMapped, modulesSize);
}

void PhysicalMemory::pushPageFrame(paddr_t physicalAddress) {
    assert(physicalAddress);
    assert(!(physicalAddress & 0xFFF));
    AutoLock lock(&mutex);

    if (((vaddr_t) (stack + 1) & 0xFFF) == 0) {
        paddr_t* stackPage = (paddr_t*) ((vaddr_t) stack & ~0xFFF);

        if (*(stackPage + 1) == 0) {
            // We need to unlock the mutex because AddressSpace::mapPhysical
            // might need to pop page frames from the stack.
            kthread_mutex_unlock(&mutex);
            vaddr_t nextStackPage = kernelSpace->mapPhysical(physicalAddress,
                    0x1000, PROT_READ | PROT_WRITE);
            kthread_mutex_lock(&mutex);

            if (unlikely(nextStackPage == 0)) {
                // If we cannot save the address, we have to leak it.
                return;
            }

            stackPage = (paddr_t*) ((vaddr_t) stack & ~0xFFF);
            *((paddr_t*) lastStackPage + 1) = nextStackPage;
            *(vaddr_t*) nextStackPage = lastStackPage;
            *((paddr_t*) nextStackPage + 1) = 0;
            lastStackPage = nextStackPage;
            return;
        } else {
            stack = (paddr_t*) *(stackPage + 1) + 1;
        }
    }

    *++stack = physicalAddress;
}

paddr_t PhysicalMemory::popPageFrame() {
    AutoLock lock(&mutex);

    paddr_t* stackPage = (paddr_t*) ((vaddr_t) stack & ~0xFFF);

    if (((vaddr_t) stack & 0xFFF) < 2 * sizeof(paddr_t)) {
        if (unlikely(*stackPage == 0)) {
            // We cannot unmap the pages of the stack anymore because
            // kernelSpace might be locked at this point.
            return 0;
        }

        stack = (paddr_t*) (*stackPage + 0x1000 - sizeof(paddr_t));
    }

    return *stack--;
}
