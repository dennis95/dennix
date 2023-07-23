/* Copyright (c) 2016, 2017, 2018, 2019, 2020, 2021, 2023 Dennis Wölfing
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
#include <dennix/kernel/cache.h>
#include <dennix/kernel/kthread.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/physicalmemory.h>
#include <dennix/kernel/syscall.h>

class MemoryStack {
public:
    MemoryStack(void* firstStackPage);
    ~MemoryStack() = default;
    NOT_COPYABLE(MemoryStack);
    NOT_MOVABLE(MemoryStack);

    void pushPageFrame(paddr_t physicalAddress, bool cache = false);
    paddr_t popPageFrame(bool cache = false);
public:
    size_t framesOnStack;
private:
    paddr_t* stack;
    vaddr_t lastStackPage;
};

static CacheController* firstCache;
static char firstStackPage[PAGESIZE] ALIGNED(PAGESIZE);
static size_t framesAvailable;
static size_t framesReserved;
static MemoryStack memstack(firstStackPage);
static size_t totalFrames;

static kthread_mutex_t mutex = KTHREAD_MUTEX_INITIALIZER;

#ifdef __x86_64__
static char firstStackPage32[PAGESIZE] ALIGNED(PAGESIZE);
static MemoryStack memstack32(firstStackPage32);

#define totalFramesOnStack (memstack.framesOnStack + memstack32.framesOnStack)
#else
#define totalFramesOnStack (memstack.framesOnStack)
#endif

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

MemoryStack::MemoryStack(void* firstStackPage) {
    stack = (paddr_t*) firstStackPage + 1;
    framesOnStack = 0;
    lastStackPage = (vaddr_t) firstStackPage;
}

void MemoryStack::pushPageFrame(paddr_t physicalAddress,
        bool cache /*= false*/) {
    if (((vaddr_t) (stack + 1) & PAGE_MISALIGN) == 0) {
        paddr_t* stackPage = (paddr_t*) ((vaddr_t) stack & ~PAGE_MISALIGN);

        if (*(stackPage + 1) == 0) {
            // We need to unlock the mutex because AddressSpace::mapPhysical
            // might need to pop page frames from the stack.
            kthread_mutex_unlock(&mutex);
            vaddr_t nextStackPage = kernelSpace->mapPhysical(physicalAddress,
                    PAGESIZE, PROT_READ | PROT_WRITE);
            kthread_mutex_lock(&mutex);

            if (cache) framesAvailable--;

            if (unlikely(nextStackPage == 0)) {
                // If we cannot save the address, we have to leak it.
                return;
            }

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
    framesOnStack++;
    if (!cache) {
        framesAvailable++;
    }
}

void PhysicalMemory::pushPageFrame(paddr_t physicalAddress) {
    assert(physicalAddress);
    assert(PAGE_ALIGNED(physicalAddress));
    AutoLock lock(&mutex);

#ifdef __x86_64__
    if (physicalAddress <= 0xFFFFF000) {
        memstack32.pushPageFrame(physicalAddress);
        return;
    }
#endif
    memstack.pushPageFrame(physicalAddress);
}

paddr_t MemoryStack::popPageFrame(bool cache /*= false*/) {
    if (((vaddr_t) stack & PAGE_MISALIGN) < 2 * sizeof(paddr_t)) {
        paddr_t* stackPage = (paddr_t*) ((vaddr_t) stack & ~PAGE_MISALIGN);
        assert(*stackPage != 0);
        stack = (paddr_t*) (*stackPage + PAGESIZE - sizeof(paddr_t));
    }

    framesOnStack--;
    if (!cache) {
        framesAvailable--;
    }
    return *stack--;
}

paddr_t PhysicalMemory::popPageFrame() {
    AutoLock lock(&mutex);
    if (framesAvailable - framesReserved == 0) return 0;

    if (totalFramesOnStack - framesReserved > 0) {
        if (memstack.framesOnStack > 0) {
            return memstack.popPageFrame();
        }

    #ifdef __x86_64__
        return memstack32.popPageFrame();
    #endif
    }

    for (CacheController* cache = firstCache; cache; cache = cache->nextCache) {
        paddr_t result = cache->reclaimCache();
        if (result) return result;
    }

    return 0;
}

#ifdef __x86_64__
paddr_t PhysicalMemory::popPageFrame32() {
    AutoLock lock(&mutex);
    if (memstack32.framesOnStack == 0) return 0;
    return memstack32.popPageFrame();
}
#else
paddr_t PhysicalMemory::popPageFrame32() {
    return popPageFrame();
}
#endif

paddr_t PhysicalMemory::popReserved() {
    AutoLock lock(&mutex);
    assert(framesReserved > 0);

    framesReserved--;
#ifdef __x86_64__
    if (memstack.framesOnStack > 0) {
        return memstack.popPageFrame();
    }
    return memstack32.popPageFrame();
#else
    return memstack.popPageFrame();
#endif
}

bool PhysicalMemory::reserveFrames(size_t frames) {
    AutoLock lock(&mutex);

    if (framesAvailable - framesReserved < frames) return false;

    // Make sure that reserved frames on the stack because memory used for
    // caching can be unreclaimable for a short time frame.
    while (totalFramesOnStack < framesReserved + frames) {
        paddr_t address = 0;
        for (CacheController* cache = firstCache; cache;
                cache = cache->nextCache) {
            address = cache->reclaimCache();
            if (address) break;
        }

        if (address) {
#ifdef __x86_64__
            if (address <= 0xFFFFF000) {
                memstack32.pushPageFrame(address, true);
            } else
#endif
            memstack.pushPageFrame(address, true);
        } else {
            return false;
        }
    }

    framesReserved += frames;
    return true;
}

void PhysicalMemory::unreserveFrames(size_t frames) {
    AutoLock lock(&mutex);

    assert(framesReserved >= frames);
    framesReserved -= frames;
}

CacheController::CacheController() {
    nextCache = firstCache;
    firstCache = this;
}

paddr_t CacheController::allocateCache() {
    AutoLock lock(&mutex);
    if (framesAvailable - framesReserved == 0) {
        return 0;
    }

    if (totalFramesOnStack - framesReserved > 0) {
        if (memstack.framesOnStack > 0) {
            return memstack.popPageFrame(true);
        }
#ifdef __x86_64__
        return memstack32.popPageFrame(true);
#endif
    }

    for (CacheController* cache = firstCache; cache; cache = cache->nextCache) {
        if (cache == this) continue;
        paddr_t result = cache->reclaimCache();
        if (result) return result;
    }

    return reclaimCache();
}

void CacheController::returnCache(paddr_t address) {
    AutoLock lock(&mutex);
    memstack.pushPageFrame(address, true);
}

void Syscall::meminfo(struct meminfo* info) {
    AutoLock lock(&mutex);
    info->mem_total = totalFrames * PAGESIZE;
    info->mem_free = totalFramesOnStack * PAGESIZE;
    info->mem_available = framesAvailable * PAGESIZE;
    info->__reserved = 0;
}
