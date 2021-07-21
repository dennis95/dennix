/* Copyright (c) 2016, 2017, 2018, 2019, 2020, 2021 Dennis Wölfing
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
#include <dennix/meminfo.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/kthread.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/physicalmemory.h>
#include <dennix/kernel/syscall.h>

static char firstStackPage[PAGESIZE] ALIGNED(PAGESIZE);
static size_t framesAvailable;
static size_t framesReserved;
static paddr_t* stack = (paddr_t*) firstStackPage + 1;
static size_t totalFrames;
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
        const multiboot_info* multiboot) {
    uintptr_t p = (uintptr_t) multiboot + 8;

    while (true) {
        const multiboot_tag* tag = (const multiboot_tag*) p;
        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            const multiboot_tag_module* moduleTag =
                    (const multiboot_tag_module*) tag;
            if (physicalAddress >= moduleTag->mod_start &&
                    physicalAddress < moduleTag->mod_end) {
                return true;
            }
        }

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            return false;
        }

        p = ALIGNUP(p + tag->size, 8);
    }
}

static inline bool isUsedByMultiboot(paddr_t physicalAddress,
        paddr_t multibootPhys, paddr_t multibootEnd) {
    return physicalAddress >= multibootPhys && physicalAddress < multibootEnd;
}

void PhysicalMemory::initialize(const multiboot_info* multiboot) {
    uintptr_t p = (uintptr_t) multiboot + 8;
    const multiboot_tag* tag;

    while (true) {
        tag = (const multiboot_tag*) p;
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            break;
        }

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            PANIC("Bootloader did not provide a memory map.");
        }

        p = ALIGNUP(p + tag->size, 8);
    }

    const multiboot_tag_mmap* mmapTag = (const multiboot_tag_mmap*) tag;

    vaddr_t mmap = (vaddr_t) mmapTag->entries;
    vaddr_t mmapEnd = mmap + (tag->size - sizeof(*mmapTag));

    paddr_t multibootPhys = kernelSpace->getPhysicalAddress(
            (vaddr_t) multiboot & ~PAGE_MISALIGN);
    paddr_t multibootEnd = multibootPhys + ALIGNUP(multiboot->total_size +
            ((vaddr_t) multiboot & PAGE_MISALIGN), PAGESIZE);

    while (mmap < mmapEnd) {
        multiboot_mmap_entry* mmapEntry = (multiboot_mmap_entry*) mmap;

        if (mmapEntry->type == MULTIBOOT_MEMORY_AVAILABLE &&
                mmapEntry->addr + mmapEntry->len <= UINTPTR_MAX) {
            paddr_t addr = (paddr_t) mmapEntry->addr;
            for (uint64_t i = 0; i < mmapEntry->len; i += PAGESIZE) {
                totalFrames++;
                if (isUsedByModule(addr + i, multiboot) ||
                        isUsedByKernel(addr + i) ||
                        isUsedByMultiboot(addr + i, multibootPhys,
                        multibootEnd)) {
                    continue;
                }

                pushPageFrame(addr + i);
            }
        }

        mmap += mmapTag->entry_size;
    }
}

void PhysicalMemory::pushPageFrame(paddr_t physicalAddress) {
    assert(physicalAddress);
    assert(PAGE_ALIGNED(physicalAddress));
    AutoLock lock(&mutex);

    if (((vaddr_t) (stack + 1) & PAGE_MISALIGN) == 0) {
        paddr_t* stackPage = (paddr_t*) ((vaddr_t) stack & ~PAGE_MISALIGN);

        if (*(stackPage + 1) == 0) {
            // We need to unlock the mutex because AddressSpace::mapPhysical
            // might need to pop page frames from the stack.
            kthread_mutex_unlock(&mutex);
            vaddr_t nextStackPage = kernelSpace->mapPhysical(physicalAddress,
                    PAGESIZE, PROT_READ | PROT_WRITE);
            kthread_mutex_lock(&mutex);

            if (unlikely(nextStackPage == 0)) {
                // If we cannot save the address, we have to leak it.
                return;
            }

            stackPage = (paddr_t*) ((vaddr_t) stack & ~PAGE_MISALIGN);
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
    framesAvailable++;
}

static paddr_t popPageFrameInternal() {
    if (((vaddr_t) stack & PAGE_MISALIGN) < 2 * sizeof(paddr_t)) {
        paddr_t* stackPage = (paddr_t*) ((vaddr_t) stack & ~PAGE_MISALIGN);
        assert(*stackPage != 0);
        stack = (paddr_t*) (*stackPage + PAGESIZE - sizeof(paddr_t));
    }

    return *stack--;
}

paddr_t PhysicalMemory::popPageFrame() {
    AutoLock lock(&mutex);
    if (framesAvailable == 0) return 0;

    framesAvailable--;
    return popPageFrameInternal();
}

paddr_t PhysicalMemory::popReserved() {
    AutoLock lock(&mutex);
    assert(framesReserved > 0);

    framesReserved--;
    return popPageFrameInternal();
}

bool PhysicalMemory::reserveFrames(size_t frames) {
    AutoLock lock(&mutex);

    if (framesAvailable < frames) return false;
    framesAvailable -= frames;
    framesReserved += frames;
    return true;
}

void PhysicalMemory::unreserveFrames(size_t frames) {
    AutoLock lock(&mutex);

    assert(framesReserved >= frames);
    framesReserved -= frames;
}

void Syscall::meminfo(struct meminfo* info) {
    AutoLock lock(&mutex);
    info->mem_total = totalFrames * PAGESIZE;
    info->mem_free = framesAvailable * PAGESIZE;
}
