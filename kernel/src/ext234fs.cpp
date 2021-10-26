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

/* kernel/src/ext234fs.cpp
 * ext2/ext3/ext4 filesystem driver.
 */

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <dennix/fs.h>
#include <dennix/kernel/ext234.h>
#include <dennix/kernel/ext234fs.h>

// This implements mostly ext2 with a hint of ext4. Any filesystem formatted for
// ext2 or ext3 should be supported unless special options were used during
// filesystem creation.
// TODO: We need to implement extent trees in order to properly use all ext4
// 64bit features.

#define ffs(x) __builtin_ffs(x)
#define min(x, y) ((x) < (y) ? (x) : (y))

FileSystem* Ext234::initialize(const Reference<Vnode>& device,
        const Reference<Vnode>& mountPoint, const char* mountPath, int flags) {
    SuperBlock superBlock;
    ssize_t bytesRead = device->pread(&superBlock, sizeof(superBlock), 1024, 0);
    if (bytesRead < 0) return nullptr;
    if (bytesRead != sizeof(superBlock) || superBlock.s_magic != 0xEF53) {
        errno = EINVAL;
        return nullptr;
    }

    if (superBlock.s_feature_incompat & ~SUPPORTED_INCOMPAT_FEATURES) {
        errno = ENOTSUP;
        return nullptr;
    }

    bool readonly = flags & MOUNT_READONLY;
    if (!readonly && superBlock.s_feature_ro_compat & ~SUPPORTED_RO_FEATURES) {
        errno = EROFS;
        return nullptr;
    }

    if (!readonly) {
        struct timespec now;
        Clock::get(CLOCK_REALTIME)->getTime(&now);
        superBlock.s_mtime = now.tv_sec;
        superBlock.s_state = superBlock.s_state & ~STATE_CLEAN;

        if (superBlock.s_rev_level >= 1) {
            strlcpy(superBlock.s_last_mounted, mountPath,
                    sizeof(superBlock.s_last_mounted));
        }

        if (device->pwrite(&superBlock, sizeof(superBlock), 1024, 0) !=
                sizeof(SuperBlock) || device->sync(0) != 0) {
            return nullptr;
        }
    }

    return new Ext234Fs(device, &superBlock, mountPoint, readonly);
}

Ext234Fs::Ext234Fs(const Reference<Vnode>& device, const SuperBlock* superBlock,
        const Reference<Vnode>& mountPoint, bool readonly)
        : mountPoint(mountPoint), readonly(readonly), device(device),
        vnodes(sizeof(vnodesBuffer) / sizeof(vnodesBuffer[0]), vnodesBuffer) {

    memcpy(&this->superBlock, superBlock, sizeof(SuperBlock));
    blockSize = 1024 << superBlock->s_log_block_size;

    uint64_t blockCount = superBlock->s_blocks_count;
    if (hasIncompatFeature(INCOMPAT_64BIT)) {
        blockCount |= (uint64_t) superBlock->s_blocks_count_hi << 32;
    }
    groupCount = ALIGNUP(blockCount, superBlock->s_blocks_per_group)
            / superBlock->s_blocks_per_group;

    gdtSize = 32;
    if (hasIncompatFeature(INCOMPAT_64BIT)) {
        gdtSize = superBlock->s_desc_size;
    }
    inodeSize = 128;
    if (superBlock->s_rev_level >= 1) {
        inodeSize = superBlock->s_inode_size;
    }

    dev = device->stat().st_rdev;
    mutex = KTHREAD_MUTEX_INITIALIZER;
    openVnodes = 0;
    renameMutex = KTHREAD_MUTEX_INITIALIZER;
}

uint64_t Ext234Fs::allocateBlock(uint64_t blockGroup) {
    BlockGroupDescriptor bg;
    if (!readBlockGroupDesc(blockGroup, &bg)) return 0;

    uint32_t freeBlocks = bg.bg_free_blocks_count;
    if (gdtSize > 32) {
        freeBlocks |= bg.bg_free_blocks_count_hi << 16;
    }

    if (freeBlocks > 0) {
        return allocateBlockInGroup(blockGroup, &bg, freeBlocks);
    }

    for (blockGroup = 0; blockGroup < groupCount; blockGroup++) {
        if (!readBlockGroupDesc(blockGroup, &bg)) return 0;

        freeBlocks = bg.bg_free_blocks_count;
        if (gdtSize > 32) {
            freeBlocks |= bg.bg_free_blocks_count_hi << 16;
        }

        if (freeBlocks > 0) {
            return allocateBlockInGroup(blockGroup, &bg, freeBlocks);
        }
    }
    errno = ENOSPC;
    return 0;
}

uint64_t Ext234Fs::allocateBlockInGroup(uint64_t blockGroup,
        BlockGroupDescriptor* bg, uint32_t freeBlocks) {
    uint64_t bitmap = bg->bg_block_bitmap;
    if (gdtSize > 32) {
        bitmap |= (uint64_t) bg->bg_block_bitmap_hi << 32;
    }
    uint64_t bitmapAddress = bitmap * blockSize;

    char* block = new char[blockSize];
    if (!block) return 0;
    if (!read(block, blockSize, bitmapAddress)) {
        delete[] block;
        return 0;
    }

    unsigned int* p = (unsigned int*) block;
    for (size_t i = 0; i < blockSize / sizeof(unsigned int); i++) {
        if (p[i] != UINT_MAX) {
            int x = ffs(~p[i]) - 1;
            uint64_t blockNumber = blockGroup * superBlock.s_blocks_per_group +
                    (blockSize == 1024);
            blockNumber += i * sizeof(unsigned int) * 8 + x;
            p[i] |= 1U << x;
            if (!write(block, blockSize, bitmapAddress)) {
                delete[] block;
                return 0;
            }
            delete[] block;

            freeBlocks--;
            bg->bg_free_blocks_count = freeBlocks & 0xFFFF;
            bg->bg_free_blocks_count_hi = freeBlocks >> 16;

            size_t descriptorSize = min(gdtSize, sizeof(BlockGroupDescriptor));
            if (!write(bg, descriptorSize, ALIGNUP(2048, blockSize) +
                    blockGroup * gdtSize)) {
                return 0;
            }

            uint64_t freeBlocksTotal = superBlock.s_free_blocks_count;
            if (hasIncompatFeature(INCOMPAT_64BIT)) {
                freeBlocksTotal |=
                        (uint64_t) superBlock.s_free_blocks_count_hi << 32;
            }
            freeBlocksTotal--;
            superBlock.s_free_blocks_count = freeBlocksTotal & 0xFFFFFFFF;
            superBlock.s_free_blocks_count_hi = freeBlocksTotal >> 32;

            return blockNumber;
        }
    }

    delete[] block;
    errno = ENOSPC;
    return 0;
}

ino_t Ext234Fs::allocateInode(uint64_t blockGroup, bool dir) {
    BlockGroupDescriptor bg;
    if (!readBlockGroupDesc(blockGroup, &bg)) return 0;

    uint32_t freeInodes = bg.bg_free_inodes_count;
    if (gdtSize > 32) {
        freeInodes |= bg.bg_free_inodes_count_hi << 16;
    }

    if (freeInodes > 0) {
        return allocateInodeInGroup(blockGroup, &bg, freeInodes, dir);
    }

    for (blockGroup = 0; blockGroup < groupCount; blockGroup++) {
        if (!readBlockGroupDesc(blockGroup, &bg)) return 0;

        freeInodes = bg.bg_free_inodes_count;
        if (gdtSize > 32) {
            freeInodes |= bg.bg_free_inodes_count_hi << 16;
        }

        if (freeInodes > 0) {
            return allocateInodeInGroup(blockGroup, &bg, freeInodes, dir);
        }
    }
    errno = ENOSPC;
    return 0;
}

ino_t Ext234Fs::allocateInodeInGroup(uint64_t blockGroup,
        BlockGroupDescriptor* bg, uint32_t freeInodes, bool dir) {
    uint64_t bitmap = bg->bg_inode_bitmap;
    if (gdtSize > 32) {
        bitmap |= (uint64_t) bg->bg_inode_bitmap_hi << 32;
    }
    uint64_t bitmapAddress = bitmap * blockSize;

    char* block = new char[blockSize];
    if (!block) return 0;
    if (!read(block, blockSize, bitmapAddress)) {
        delete[] block;
        return 0;
    }

    unsigned int* p = (unsigned int*) block;
    for (size_t i = 0; i < blockSize / sizeof(unsigned int); i++) {
        if (p[i] != UINT_MAX) {
            int x = ffs(~p[i]) - 1;
            uint64_t inodeNumber = blockGroup * superBlock.s_inodes_per_group +
                    1;
            inodeNumber += i * sizeof(unsigned int) * 8 + x;
            p[i] |= 1U << x;
            if (!write(block, blockSize, bitmapAddress)) {
                delete[] block;
                return 0;
            }
            delete[] block;

            freeInodes--;
            bg->bg_free_inodes_count = freeInodes & 0xFFFF;
            bg->bg_free_inodes_count_hi = freeInodes >> 16;

            if (dir) {
                uint32_t usedDirs = bg->bg_used_dirs_count;
                if (gdtSize > 32) {
                    usedDirs |= (uint32_t) bg->bg_used_dirs_count_hi << 16;
                }
                usedDirs++;
                bg->bg_used_dirs_count = usedDirs & 0xFFFF;
                bg->bg_used_dirs_count_hi = usedDirs >> 16;
            }

            size_t descriptorSize = min(gdtSize, sizeof(BlockGroupDescriptor));
            if (!write(bg, descriptorSize, ALIGNUP(2048, blockSize) +
                    blockGroup * gdtSize)) {
                return 0;
            }

            superBlock.s_free_inodes_count = superBlock.s_free_inodes_count - 1;

            return inodeNumber;
        }
    }

    delete[] block;
    errno = ENOSPC;
    return 0;
}

ino_t Ext234Fs::createInode(uint64_t blockGroup, mode_t mode) {
    ino_t ino = allocateInode(blockGroup, S_ISDIR(mode));
    if (!ino) return 0;
    Inode inode = {};
    inode.i_mode = mode;
    blockGroup = getBlockGroup(ino);

    BlockGroupDescriptor bg;
    if (!readBlockGroupDesc(blockGroup, &bg)) return false;
    uint64_t inodeTable = bg.bg_inode_table;
    if (gdtSize > 32) {
        inodeTable |= (uint64_t) bg.bg_inode_table_hi << 32;
    }

    uint64_t localIndex = (ino - 1) % superBlock.s_inodes_per_group;
    uint64_t inodeAddress = inodeTable * blockSize + (localIndex * inodeSize);
    if (!writeInode(&inode, inodeAddress)) return 0;
    return ino;
}

bool Ext234Fs::deallocateBlock(uint64_t blockNumber) {
    if (blockSize == 1024) {
        blockNumber--;
    }
    uint64_t blockGroup = blockNumber / superBlock.s_blocks_per_group;

    BlockGroupDescriptor bg;
    if (!readBlockGroupDesc(blockGroup, &bg)) return false;

    uint32_t freeBlocks = bg.bg_free_blocks_count;
    if (gdtSize > 32) {
        freeBlocks |= bg.bg_free_blocks_count_hi << 16;
    }

    uint64_t bitmap = bg.bg_block_bitmap;
    if (gdtSize > 32) {
        bitmap |= (uint64_t) bg.bg_block_bitmap_hi << 32;
    }
    uint64_t bitmapAddress = bitmap * blockSize;

    uint64_t localIndex = blockNumber % superBlock.s_blocks_per_group;
    size_t index = localIndex / 8;
    unsigned char entry;
    if (!read(&entry, 1, bitmapAddress + index)) return false;
    entry &= ~(1U << (localIndex % 8));
    if (!write(&entry, 1, bitmapAddress + index)) return false;
    freeBlocks++;
    bg.bg_free_blocks_count = freeBlocks & 0xFFFF;
    bg.bg_free_blocks_count_hi = freeBlocks >> 16;

    size_t descriptorSize = min(gdtSize, sizeof(BlockGroupDescriptor));
    if (!write(&bg, descriptorSize, ALIGNUP(2048, blockSize) +
            blockGroup * gdtSize)) {
        return false;
    }

    uint64_t freeBlocksTotal = superBlock.s_free_blocks_count;
    if (hasIncompatFeature(INCOMPAT_64BIT)) {
        freeBlocksTotal |= (uint64_t) superBlock.s_free_blocks_count_hi << 32;
    }
    freeBlocksTotal++;
    superBlock.s_free_blocks_count = freeBlocksTotal & 0xFFFFFFFF;
    superBlock.s_free_blocks_count_hi = freeBlocksTotal >> 32;

    return true;
}

bool Ext234Fs::deallocateInode(ino_t ino, bool dir) {
    uint64_t blockGroup = getBlockGroup(ino);

    BlockGroupDescriptor bg;
    if (!readBlockGroupDesc(blockGroup, &bg)) return false;

    uint32_t freeInodes = bg.bg_free_inodes_count;
    if (gdtSize > 32) {
        freeInodes |= bg.bg_free_inodes_count_hi << 16;
    }

    uint64_t bitmap = bg.bg_inode_bitmap;
    if (gdtSize > 32) {
        bitmap |= (uint64_t) bg.bg_inode_bitmap_hi << 32;
    }
    uint64_t bitmapAddress = bitmap * blockSize;

    uint64_t localIndex = (ino - 1) % superBlock.s_inodes_per_group;
    size_t index = localIndex / 8;
    unsigned char entry;
    if (!read(&entry, 1, bitmapAddress + index)) return false;
    entry &= ~(1U << (localIndex % 8));
    if (!write(&entry, 1, bitmapAddress + index)) return false;
    freeInodes++;
    bg.bg_free_inodes_count = freeInodes & 0xFFFF;
    bg.bg_free_inodes_count_hi = freeInodes >> 16;

    if (dir) {
        uint32_t usedDirs = bg.bg_used_dirs_count;
        if (gdtSize > 32) {
            usedDirs |= (uint32_t) bg.bg_used_dirs_count_hi << 16;
        }
        usedDirs--;
        bg.bg_used_dirs_count = usedDirs & 0xFFFF;
        bg.bg_used_dirs_count_hi = usedDirs >> 16;
    }

    size_t descriptorSize = min(gdtSize, sizeof(BlockGroupDescriptor));
    if (!write(&bg, descriptorSize, ALIGNUP(2048, blockSize) +
            blockGroup * gdtSize)) {
        return false;
    }

    superBlock.s_free_inodes_count = superBlock.s_free_inodes_count + 1;

    return true;
}

bool Ext234Fs::decreaseInodeBlockCount(Inode* inode,
        uint64_t oldBlockCount, uint64_t newBlockCount) {
    size_t indirectBlockPointers = blockSize / 4;
    size_t doublyIndirectPointers = indirectBlockPointers *
            indirectBlockPointers;

    uint64_t currentBlockCount = oldBlockCount;
    while (currentBlockCount > newBlockCount) {
        uint64_t block = currentBlockCount - 1;

        little_uint32_t blockNum;

        if (block >= 12 + indirectBlockPointers + doublyIndirectPointers) {
            blockNum = inode->i_block[14];
            block -= 12 + indirectBlockPointers + doublyIndirectPointers;

            if (block == 0) {
                if (!deallocateBlock(blockNum)) return false;
                inode->i_block[14] = 0;
            }

            size_t index = block / doublyIndirectPointers;
            block = block % doublyIndirectPointers;

            uint64_t address = blockNum * blockSize + index * 4;
            if (!read(&blockNum, sizeof(blockNum), address)) return false;

            if (block == 0) {
                if (!deallocateBlock(blockNum)) return false;
                little_uint32_t b = 0;
                if (!write(&b, sizeof(b), address)) return false;
            }

            goto doublyIndirect;
        } else if (block >= 12 + indirectBlockPointers) {
            blockNum = inode->i_block[13];
            block -= 12 + indirectBlockPointers;

            if (block == 0) {
                if (!deallocateBlock(blockNum)) return false;
                inode->i_block[13] = 0;
            }
doublyIndirect:
            size_t index = block / indirectBlockPointers;
            block = block % indirectBlockPointers;

            uint64_t address = blockNum * blockSize + index * 4;
            if (!read(&blockNum, sizeof(blockNum), address)) return false;

            if (block == 0) {
                if (!deallocateBlock(blockNum)) return false;
                little_uint32_t b = 0;
                if (!write(&b, sizeof(b), address)) return false;
            }

            goto indirect;
        } else if (block >= 12) {
            block -= 12;
            blockNum = inode->i_block[12];

            if (block == 0) {
                if (!deallocateBlock(blockNum)) return false;
                inode->i_block[12] = 0;
            }
indirect:
            uint64_t address = blockNum * blockSize + block * 4;
            if (!read(&blockNum, sizeof(blockNum), address)) return false;
            if (!deallocateBlock(blockNum)) return false;
            blockNum = 0;
            if (!write(&blockNum, sizeof(blockNum), address)) return false;
        } else {
            if (!deallocateBlock(inode->i_block[block])) return false;
            inode->i_block[block] = 0;
        }

        currentBlockCount--;
    }

    return true;
}

void Ext234Fs::dropVnodeReference(ino_t ino) {
    kthread_mutex_lock(&mutex);

    Vnode* vnode = vnodes.get(ino);
    if (vnode && vnode->getRefCount() == 1) {
        // Only the reference being dropped exists.
        vnodes.remove(ino);
        openVnodes--;
    }
    // The mutex will be released in finishDropVnodeReference().
}

void Ext234Fs::finishDropVnodeReference() {
    // The mutex was acquired in dropVnodeReference().
    kthread_mutex_unlock(&mutex);
}

uint64_t Ext234Fs::getBlockCount(uint64_t fileSize) {
    size_t indirectBlockPointers = blockSize / 4;
    uint64_t dataBlocks = ALIGNUP(fileSize, blockSize) / blockSize;
    uint64_t indirection1 = dataBlocks > 12 ? ALIGNUP(dataBlocks - 12,
            indirectBlockPointers) / indirectBlockPointers : 0;
    uint64_t indirection2 = indirection1 > 1 ?  ALIGNUP(indirection1 - 1,
            indirectBlockPointers) / indirectBlockPointers : 0;
    uint64_t indirection3 = indirection2 > 1 ?  ALIGNUP(indirection2 - 1,
            indirectBlockPointers) / indirectBlockPointers : 0;

    return dataBlocks + indirection1 + indirection2 + indirection3;
}

uint64_t Ext234Fs::getBlockGroup(ino_t ino) {
    return (ino - 1) / superBlock.s_inodes_per_group;
}

uint64_t Ext234Fs::getInodeBlockAddress(const Inode* inode, uint64_t block) {
    size_t indirectBlockPointers = blockSize / 4;
    size_t doublyIndirectPointers = indirectBlockPointers *
            indirectBlockPointers;

    little_uint32_t blockNum;

    if (block >= 12 + indirectBlockPointers + doublyIndirectPointers) {
        blockNum = inode->i_block[14];
        block -= 12 + indirectBlockPointers + doublyIndirectPointers;

        size_t index = block / doublyIndirectPointers;
        block = block % doublyIndirectPointers;

        uint64_t address = blockNum * blockSize + index * 4;
        if (!read(&blockNum, sizeof(blockNum), address)) return -1;

        goto doublyIndirect;
    } else if (block >= 12 + indirectBlockPointers) {
        blockNum = inode->i_block[13];
        block -= 12 + indirectBlockPointers;

doublyIndirect:
        size_t index = block / indirectBlockPointers;
        block = block % indirectBlockPointers;

        uint64_t address = blockNum * blockSize + index * 4;
        if (!read(&blockNum, sizeof(blockNum), address)) return -1;

        goto indirect;
    } else if (block >= 12) {
        block -= 12;
        blockNum = inode->i_block[12];

indirect:
        uint64_t address = blockNum * blockSize + block * 4;
        if (!read(&blockNum, sizeof(blockNum), address)) return -1;

        return blockNum * blockSize;
    } else {
        return inode->i_block[block] * blockSize;
    }
}

struct timespec Ext234Fs::getInodeATime(const Inode* inode) {
    struct timespec time;
    time.tv_sec = (int32_t) inode->i_atime;
    time.tv_nsec = 0;

    if (inodeSize > 128 && inode->i_extra_isize + 128U >=
            offsetof(Inode, i_atime_extra) + sizeof(inode->i_atime_extra)) {
        time.tv_sec += (uint64_t) (inode->i_atime_extra & 0x3) << 32;
        time.tv_nsec = inode->i_atime_extra >> 2;
    }

    return time;
}

struct timespec Ext234Fs::getInodeCTime(const Inode* inode) {
    struct timespec time;
    time.tv_sec = (int32_t) inode->i_ctime;
    time.tv_nsec = 0;

    if (inodeSize > 128 && inode->i_extra_isize + 128U >=
            offsetof(Inode, i_ctime_extra) + sizeof(inode->i_ctime_extra)) {
        time.tv_sec += (uint64_t) (inode->i_ctime_extra & 0x3) << 32;
        time.tv_nsec = inode->i_ctime_extra >> 2;
    }

    return time;
}

struct timespec Ext234Fs::getInodeMTime(const Inode* inode) {
    struct timespec time;
    time.tv_sec = (int32_t) inode->i_mtime;
    time.tv_nsec = 0;

    if (inodeSize > 128 && inode->i_extra_isize + 128U >=
            offsetof(Inode, i_mtime_extra) + sizeof(inode->i_mtime_extra)) {
        time.tv_sec += (uint64_t) (inode->i_mtime_extra & 0x3) << 32;
        time.tv_nsec = inode->i_mtime_extra >> 2;
    }

    return time;
}

uint64_t Ext234Fs::getInodeSize(const Inode* inode) {
    uint64_t size = inode->i_size;
    if (hasReadOnlyFeature(RO_COMPAT_LARGE_FILE)) {
        size |= (uint64_t) inode->i_size_high << 32;
    }
    return size;
}

Reference<Vnode> Ext234Fs::getRootDir() {
    return getVnode(2);
}

Reference<Ext234Vnode> Ext234Fs::getVnode(ino_t ino) {
    kthread_mutex_lock(&mutex);
    // The mutex must be unlocked before returning. Otherwise dropping the
    // reference to vnode might deadlock when trying to remove the entry from
    // the vnode table.

    Reference<Ext234Vnode> vnode = vnodes.get(ino);
    if (vnode) {
        kthread_mutex_unlock(&mutex);
        return vnode;
    }

    Inode inode;
    uint64_t inodeAddress;
    if (!readInode(ino, &inode, inodeAddress)) {
        kthread_mutex_unlock(&mutex);
        return nullptr;
    }
    vnode = new Ext234Vnode(this, ino, &inode, inodeAddress);
    if (!vnode) {
        kthread_mutex_unlock(&mutex);
        return nullptr;
    }
    vnodes.add((Ext234Vnode*) vnode);
    openVnodes++;
    kthread_mutex_unlock(&mutex);
    return vnode;
}

Reference<Ext234Vnode> Ext234Fs::getVnodeIfOpen(ino_t ino) {
    kthread_mutex_lock(&mutex);
    Reference<Ext234Vnode> vnode = vnodes.get(ino);
    kthread_mutex_unlock(&mutex);
    return vnode;
}

bool Ext234Fs::hasIncompatFeature(uint32_t feature) {
    if (superBlock.s_rev_level == 0) return false;
    return (superBlock.s_feature_incompat & feature) == feature;
}

bool Ext234Fs::hasReadOnlyFeature(uint32_t feature) {
    if (superBlock.s_rev_level == 0) return false;
    return (superBlock.s_feature_ro_compat & feature) == feature;
}

bool Ext234Fs::increaseInodeBlockCount(ino_t ino, Inode* inode,
        uint64_t oldBlockCount, uint64_t newBlockCount) {
    size_t indirectBlockPointers = blockSize / 4;
    size_t doublyIndirectPointers = indirectBlockPointers *
            indirectBlockPointers;

    uint64_t blockGroup = getBlockGroup(ino);

    uint64_t currentBlockCount = oldBlockCount;
    while (currentBlockCount < newBlockCount) {
        uint64_t block = currentBlockCount;
        little_uint32_t blockNumber = allocateBlock(blockGroup);
        if (!blockNumber) goto fail;

        little_uint32_t blockNum;

        if (block >= 12 + indirectBlockPointers + doublyIndirectPointers) {
            if (!inode->i_block[14]) {
                inode->i_block[14] = allocateBlock(blockGroup);
                if (!inode->i_block[14]) goto fail;
                char* buffer = new char[blockSize];
                if (!buffer) goto fail;
                memset(buffer, 0, blockSize);
                if (!write(buffer, blockSize, inode->i_block[14] * blockSize)) {
                    delete[] buffer;
                    goto fail;
                }
                delete[] buffer;
            }

            blockNum = inode->i_block[14];
            block -= 12 + indirectBlockPointers + doublyIndirectPointers;

            size_t index = block / doublyIndirectPointers;
            block = block % doublyIndirectPointers;

            uint64_t address = blockNum * blockSize + index * 4;
            if (!read(&blockNum, sizeof(blockNum), address)) goto fail;

            if (!blockNum) {
                blockNum = allocateBlock(blockGroup);
                if (!blockNum) goto fail;
                char* buffer = new char[blockSize];
                if (!buffer) goto fail;
                memset(buffer, 0, blockSize);
                if (!write(buffer, blockSize, blockNum * blockSize) ||
                        !write(&blockNum, sizeof(blockNum), address)) {
                    delete[] buffer;
                    goto fail;
                }
                delete[] buffer;
            }

            goto doublyIndirect;
        } else if (block >= 12 + indirectBlockPointers) {
            if (!inode->i_block[13]) {
                inode->i_block[13] = allocateBlock(blockGroup);
                if (!inode->i_block[13]) goto fail;
                char* buffer = new char[blockSize];
                if (!buffer) goto fail;
                memset(buffer, 0, blockSize);
                if (!write(buffer, blockSize, inode->i_block[13] * blockSize)) {
                    delete[] buffer;
                    goto fail;
                }
                delete[] buffer;
            }

            blockNum = inode->i_block[13];
            block -= 12 + indirectBlockPointers;

doublyIndirect:
            size_t index = block / indirectBlockPointers;
            block = block % indirectBlockPointers;

            uint64_t address = blockNum * blockSize + index * 4;
            if (!read(&blockNum, sizeof(blockNum), address)) goto fail;

            if (!blockNum) {
                blockNum = allocateBlock(blockGroup);
                if (!blockNum) goto fail;
                char* buffer = new char[blockSize];
                if (!buffer) goto fail;
                memset(buffer, 0, blockSize);
                if (!write(buffer, blockSize, blockNum * blockSize) ||
                        !write(&blockNum, sizeof(blockNum), address)) {
                    delete[] buffer;
                    goto fail;
                }
                delete[] buffer;
            }

            goto indirect;
        } else if (block >= 12) {
            if (!inode->i_block[12]) {
                inode->i_block[12] = allocateBlock(blockGroup);
                if (!inode->i_block[12]) goto fail;
                char* buffer = new char[blockSize];
                if (!buffer) goto fail;
                memset(buffer, 0, blockSize);
                if (!write(buffer, blockSize, inode->i_block[12] * blockSize)) {
                    delete[] buffer;
                    goto fail;
                }
                delete[] buffer;
            }

            block -= 12;
            blockNum = inode->i_block[12];
indirect:
            uint64_t address = blockNum * blockSize + block * 4;
            if (!write(&blockNumber, sizeof(blockNumber), address)) goto fail;
        } else {
            inode->i_block[block] = blockNumber;
        }

        currentBlockCount++;
    }

    return true;

fail:
    if (currentBlockCount != oldBlockCount) {
        decreaseInodeBlockCount(inode, currentBlockCount, oldBlockCount);
    }

    return false;
}

bool Ext234Fs::onUnmount() {
    AutoLock lock(&mutex);

    if (openVnodes) {
        errno = EBUSY;
        return false;
    }

    if (!readonly) {
        struct timespec now;
        Clock::get(CLOCK_REALTIME)->getTime(&now);
        superBlock.s_wtime = now.tv_sec;
        superBlock.s_state = superBlock.s_state | STATE_CLEAN;
        writeSuperBlock();
    }

    device->sync(0);
    return true;
}

bool Ext234Fs::read(void* buffer, size_t size, off_t offset) {
    return device->pread(buffer, size, offset, 0) == (ssize_t) size;
}

bool Ext234Fs::readBlockGroupDesc(uint64_t blockGroup,
        BlockGroupDescriptor* bg) {
    uint64_t bgdt = ALIGNUP(2048, blockSize);
    uint64_t descriptorOffset = bgdt + blockGroup * gdtSize;

    size_t descriptorSize = min(gdtSize, sizeof(BlockGroupDescriptor));
    return read(bg, descriptorSize, descriptorOffset);
}

bool Ext234Fs::readInode(uint64_t ino, Inode* inode, uint64_t& inodeAddress) {
    uint64_t blockGroup = getBlockGroup(ino);
    uint64_t localIndex = (ino - 1) % superBlock.s_inodes_per_group;

    BlockGroupDescriptor bg;
    if (!readBlockGroupDesc(blockGroup, &bg)) return false;
    uint64_t inodeTable = bg.bg_inode_table;
    if (gdtSize > 32) {
        inodeTable |= (uint64_t) bg.bg_inode_table_hi << 32;
    }

    size_t size = min(inodeSize, sizeof(Inode));
    inodeAddress = inodeTable * blockSize + (localIndex * inodeSize);
    return read(inode, size, inodeAddress);
}

bool Ext234Fs::readInodeData(const Inode* inode, off_t offset, void* buffer,
        size_t size) {
    char* buf = (char*) buffer;

    while (size > 0) {
        uint64_t block = offset / blockSize;
        uint64_t misalign = offset % blockSize;
        size_t readSize = blockSize - misalign;
        if (readSize > size) readSize = size;

        uint64_t address = getInodeBlockAddress(inode, block);
        if (address == (uint64_t) -1 ||
                !read(buf, readSize, address + misalign)) {
            return false;
        }

        size -= readSize;
        offset += readSize;
        buf += readSize;
    }

    return true;
}

bool Ext234Fs::resizeInode(ino_t ino, Inode* inode, off_t newSize) {
    uint64_t oldSize = getInodeSize(inode);
    uint64_t oldBlockCount = ALIGNUP(oldSize, blockSize) / blockSize;
    uint64_t newBlockCount = ALIGNUP(newSize, blockSize) / blockSize;

    if (oldBlockCount > newBlockCount) {
        if (!decreaseInodeBlockCount(inode, oldBlockCount, newBlockCount)) {
            return false;
        }
    } else if (oldBlockCount < newBlockCount) {
        if (!increaseInodeBlockCount(ino, inode, oldBlockCount,
                newBlockCount)) {
            return false;
        }
    }

    inode->i_blocks = getBlockCount(newSize) * (blockSize / 512);
    inode->i_size = newSize;
    if (hasReadOnlyFeature(RO_COMPAT_LARGE_FILE)) {
        inode->i_size_high = newSize >> 32;
    }

    return true;
}

void Ext234Fs::setTime(struct timespec* ts, little_uint32_t* time,
        little_uint32_t* extraTime) {
    if (ts->tv_sec < -0x80000000LL) {
        ts->tv_sec = -0x80000000LL;
    }

    if (ts->tv_sec > 0x37FFFFFFF) {
        ts->tv_sec = 0x37FFFFFFF;
    }

    if (extraTime) {
        *extraTime = ts->tv_nsec << 2;
        if (ts->tv_sec > 0) {
            *extraTime = *extraTime | ((ts->tv_sec >> 32) & 0x3);
        }
    } else {
        if (ts->tv_sec > 0x7FFFFFFF) {
            ts->tv_sec = 0x7FFFFFFF;
        }
        ts->tv_nsec = 0;
    }

    if (ts->tv_sec < 0) {
        *time = (int32_t) ts->tv_sec;
    } else {
        *time = ts->tv_sec & 0x7FFFFFFF;
    }
}

int Ext234Fs::sync(int flags) {
    if (!readonly) {
        struct timespec now;
        Clock::get(CLOCK_REALTIME)->getTime(&now);
        superBlock.s_wtime = now.tv_sec;
        if (!writeSuperBlock()) return -1;
    }

    return device->sync(flags);
}

bool Ext234Fs::write(const void* buffer, size_t size, off_t offset) {
    assert(!readonly);
    return device->pwrite(buffer, size, offset, 0) == (ssize_t) size;
}

bool Ext234Fs::writeInode(const Inode* inode, uint64_t inodeAddress) {
    size_t size = min(inodeSize, sizeof(Inode));
    return write(inode, size, inodeAddress);
}

bool Ext234Fs::writeInodeData(const Inode* inode, off_t offset,
        const void* buffer, size_t size) {
    char* buf = (char*) buffer;

    while (size > 0) {
        uint64_t block = offset / blockSize;
        uint64_t misalign = offset % blockSize;
        size_t writeSize = blockSize - misalign;
        if (writeSize > size) writeSize = size;

        uint64_t address = getInodeBlockAddress(inode, block);
        if (address == (uint64_t) -1 ||
                !write(buf, writeSize, address + misalign)) {
            return false;
        }

        size -= writeSize;
        offset += writeSize;
        buf += writeSize;
    }

    return true;
}

bool Ext234Fs::writeSuperBlock() {
    return write(&superBlock, sizeof(SuperBlock), 1024);
}
