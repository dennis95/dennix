/* Copyright (c) 2021, 2023 Dennis Wölfing
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

/* kernel/include/dennix/kernel/blockcache.h
 * Cached block device.
 */

#ifndef KERNEL_BLOCKCACHE_H
#define KERNEL_BLOCKCACHE_H

#include <dennix/kernel/cache.h>
#include <dennix/kernel/hashtable.h>
#include <dennix/kernel/vnode.h>
#include <dennix/kernel/worker.h>

class BlockCacheDevice : public Vnode, public CacheController {
protected:
    BlockCacheDevice(mode_t mode, dev_t dev);
    NOT_COPYABLE(BlockCacheDevice);
    NOT_MOVABLE(BlockCacheDevice);
public:
    virtual ~BlockCacheDevice() = default;
    void freeUnusedBlocks();
    bool isSeekable() override;
    ssize_t pread(void* buffer, size_t size, off_t offset, int flags) override;
    ssize_t pwrite(const void* buffer, size_t size, off_t offset, int flags)
            override;
    paddr_t reclaimCache() override;
protected:
    virtual bool readUncached(void* buffer, size_t size, off_t offset,
            int flags) = 0;
    virtual bool writeUncached(const void* buffer, size_t size, off_t offset,
            int flags) = 0;
private:
    struct Block {
        Block(vaddr_t address, uint64_t blockNumber);
        ~Block() = default;
        NOT_COPYABLE(Block);
        NOT_MOVABLE(Block);

        vaddr_t address;
        uint64_t blockNumber;
        Block* nextInHashTable;
        Block* prevAccessed;
        Block* nextAccessed;
        Block* nextFree;

        uint64_t hashKey() { return blockNumber; }
    };
    HashTable<Block, uint64_t> blocks;
    Block* blockBuffer[10000];
    kthread_mutex_t cacheMutex;
    Block* freeList;
    Block* leastRecentlyUsed;
    Block* mostRecentlyUsed;
    WorkerJob workerJob;
private:
    void useBlock(Block* block);
};

#endif
