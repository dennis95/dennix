/* Copyright (c) 2021 Dennis Wölfing
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

/* kernel/src/blockcache.cpp
 * Cached block device.
 */

#include <errno.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/blockcache.h>
#include <dennix/kernel/interrupts.h>

static void worker(void* device) {
    BlockCacheDevice* dev = (BlockCacheDevice*) device;
    dev->freeUnusedBlocks();
}

BlockCacheDevice::BlockCacheDevice(mode_t mode, dev_t dev)
        : Vnode(mode | S_IFBLK, dev),
        blocks(sizeof(blockBuffer) / sizeof(blockBuffer[0]), blockBuffer) {
    cacheMutex = KTHREAD_MUTEX_INITIALIZER;
    freeList = nullptr;
    leastRecentlyUsed = nullptr;
    mostRecentlyUsed = nullptr;
    workerJob.func = worker;
    workerJob.context = this;
}

bool BlockCacheDevice::isSeekable() {
    return true;
}

void BlockCacheDevice::useBlock(Block* block) {
    // Remove the block from its current location.
    if (block->prevAccessed) {
        block->prevAccessed->nextAccessed = block->nextAccessed;
    } else if (block == leastRecentlyUsed) {
        leastRecentlyUsed = block->nextAccessed;
    }
    if (block->nextAccessed) {
        block->nextAccessed->prevAccessed = block->prevAccessed;
    } else if (block == mostRecentlyUsed) {
        mostRecentlyUsed = block->prevAccessed;
    }

    // Add the block as the most recently used.
    block->prevAccessed = mostRecentlyUsed;
    if (mostRecentlyUsed) {
        mostRecentlyUsed->nextAccessed = block;
    } else {
        leastRecentlyUsed = block;
    }
    mostRecentlyUsed = block;
    block->nextAccessed = nullptr;
}

ssize_t BlockCacheDevice::pread(void* buffer, size_t size, off_t offset,
        int flags) {
    if (size == 0) return 0;

    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }

    AutoLock lock(&mutex);
    if (offset >= stats.st_size) return 0;
    if ((off_t) size > stats.st_size - offset) {
        size = stats.st_size - offset;
    }

    kthread_mutex_lock(&cacheMutex);
    Block* allocatedBlock = nullptr;
    ssize_t bytesRead = 0;
    char* buf = (char*) buffer;

    while (size > 0) {
        uint64_t blockNumber = offset / PAGESIZE;
        off_t blockOffset = blockNumber * PAGESIZE;

        Block* block = blocks.get(blockNumber);
        if (!block) {
            if (!allocatedBlock) {
                kthread_mutex_unlock(&cacheMutex);

                paddr_t physicalAddress = allocateCache();
                if (!physicalAddress) {
                    if (bytesRead) return bytesRead;
                    errno = ENOMEM;
                    return -1;
                }
                vaddr_t address = kernelSpace->mapPhysical(physicalAddress,
                        PAGESIZE, PROT_READ | PROT_WRITE);
                if (!address) {
                    returnCache(physicalAddress);
                    if (bytesRead) return bytesRead;
                    errno = ENOMEM;
                    return -1;
                }
                allocatedBlock = new Block(address, blockNumber);
                if (!allocatedBlock) {
                    kernelSpace->unmapPhysical(address, PAGESIZE);
                    returnCache(physicalAddress);
                    if (bytesRead) return bytesRead;
                    return -1;
                }

                kthread_mutex_lock(&cacheMutex);
            }

            block = blocks.get(blockNumber);

            if (!block) {
                size_t readSize = PAGESIZE;
                if (unlikely(blockOffset + PAGESIZE > stats.st_size)) {
                    // The device ends before the end of the page.
                    readSize = stats.st_size - blockOffset;
                }

                if (!readUncached((void*) allocatedBlock->address, readSize,
                        blockOffset, flags)) {
                    if (!bytesRead) bytesRead = -1;
                    break;
                }

                block = allocatedBlock;
                allocatedBlock = nullptr;
                blocks.add(block);
            }
        }

        useBlock(block);

        size_t readSize = PAGESIZE - (offset & PAGE_MISALIGN);
        if (readSize > size) readSize = size;

        memcpy(buf + bytesRead, (char*) block->address +
                (offset & PAGE_MISALIGN), readSize);
        offset += readSize;
        bytesRead += readSize;
        size -= readSize;
    }

    kthread_mutex_unlock(&cacheMutex);

    if (allocatedBlock) {
        paddr_t physicalAddress = kernelSpace->getPhysicalAddress(
                allocatedBlock->address);
        kernelSpace->unmapPhysical(allocatedBlock->address, PAGESIZE);
        returnCache(physicalAddress);
        delete allocatedBlock;
    }

    return bytesRead;
}

ssize_t BlockCacheDevice::pwrite(const void* buffer, size_t size, off_t offset,
        int flags) {
    if (size == 0) return 0;

    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }

    AutoLock lock(&mutex);
    if (offset >= stats.st_size) {
        errno = ENOSPC;
        return -1;
    }
    if ((off_t) size > stats.st_size - offset) {
        size = stats.st_size - offset;
    }

    kthread_mutex_lock(&cacheMutex);
    Block* allocatedBlock = nullptr;
    ssize_t bytesWritten = 0;
    const char* buf = (const char*) buffer;

    while (size > 0) {
        uint64_t blockNumber = offset / PAGESIZE;
        off_t blockOffset = blockNumber * PAGESIZE;

        Block* block = blocks.get(blockNumber);
        if (!block) {
            if (!allocatedBlock) {
                kthread_mutex_unlock(&cacheMutex);

                paddr_t physicalAddress = allocateCache();
                if (!physicalAddress) {
                    if (bytesWritten) return bytesWritten;
                    errno = ENOMEM;
                    return -1;
                }
                vaddr_t address = kernelSpace->mapPhysical(physicalAddress,
                        PAGESIZE, PROT_READ | PROT_WRITE);
                if (!address) {
                    returnCache(physicalAddress);
                    if (bytesWritten) return bytesWritten;
                    errno = ENOMEM;
                    return -1;
                }
                allocatedBlock = new Block(address, blockNumber);
                if (!allocatedBlock) {
                    kernelSpace->unmapPhysical(address, PAGESIZE);
                    returnCache(physicalAddress);
                    if (bytesWritten) return bytesWritten;
                    return -1;
                }

                kthread_mutex_lock(&cacheMutex);
            }

            block = blocks.get(blockNumber);

            if (!block) {
                // Only read the block from the device if we are not overwriting
                // it completely.
                if (offset != blockOffset || size < PAGESIZE) {
                    size_t readSize = PAGESIZE;
                    if (unlikely(blockOffset + PAGESIZE > stats.st_size)) {
                        // The device ends before the end of the page.
                        readSize = stats.st_size - blockOffset;
                    }

                    if (!readUncached((void*) allocatedBlock->address, readSize,
                            blockOffset, flags)) {
                        if (!bytesWritten) bytesWritten = -1;
                        break;
                    }
                }

                block = allocatedBlock;
                allocatedBlock = nullptr;
                blocks.add(block);
            }
        }

        useBlock(block);

        size_t writeSize = PAGESIZE - (offset & PAGE_MISALIGN);
        if (writeSize > size) writeSize = size;

        memcpy((char*) block->address + (offset & PAGE_MISALIGN),
                buf + bytesWritten, writeSize);

        size_t writeOffset = offset & ~(stats.st_blksize - 1);
        size_t writeLength = ALIGNUP(writeSize, stats.st_blksize);

        if (!writeUncached((char*) block->address + (writeOffset &
                PAGE_MISALIGN), writeLength, writeOffset, flags)) {
            if (bytesWritten == 0) return -1;
            return bytesWritten;
        }

        offset += writeSize;
        bytesWritten += writeSize;
        size -= writeSize;
    }

    kthread_mutex_unlock(&cacheMutex);

    if (allocatedBlock) {
        paddr_t physicalAddress = kernelSpace->getPhysicalAddress(
                allocatedBlock->address);
        kernelSpace->unmapPhysical(allocatedBlock->address, PAGESIZE);
        returnCache(physicalAddress);
        delete allocatedBlock;
    }

    return bytesWritten;
}

void BlockCacheDevice::freeUnusedBlocks() {
    kthread_mutex_lock(&cacheMutex);
    Block* block = freeList;
    freeList = nullptr;
    kthread_mutex_unlock(&cacheMutex);

    while (block) {
        kernelSpace->unmapPhysical(block->address, PAGESIZE);
        Block* nextBlock = block->nextFree;
        delete block;
        block = nextBlock;
    }
}

paddr_t BlockCacheDevice::reclaimCache() {
    AutoLock lock(&cacheMutex);

    Block* block = leastRecentlyUsed;
    if (!block) return 0;

    leastRecentlyUsed = block->nextAccessed;
    if (leastRecentlyUsed) {
        leastRecentlyUsed->prevAccessed = nullptr;
    } else {
        mostRecentlyUsed = nullptr;
    }

    blocks.remove(block->blockNumber);

    block->nextFree = freeList;
    freeList = block;
    if (!block->nextFree) {
        Interrupts::disable();
        WorkerThread::addJob(&workerJob);
        Interrupts::enable();
    }

    paddr_t physicalAddress = kernelSpace->getPhysicalAddress(block->address);
    // We cannot unmap the block yet because the PMM is locked. This will be
    // handled by the worker thread.
    return physicalAddress;
}

BlockCacheDevice::Block::Block(vaddr_t address, uint64_t blockNumber) {
    this->address = address;
    this->blockNumber = blockNumber;
    prevAccessed = nullptr;
    nextAccessed = nullptr;
}
