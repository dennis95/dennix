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

/* kernel/src/partition.cpp
 * Partitions.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <dennix/poll.h>
#include <dennix/seek.h>
#include <dennix/kernel/devices.h>
#include <dennix/kernel/endian.h>
#include <dennix/kernel/partition.h>

struct GptHeader {
    char signature[8];
    little_uint32_t revision;
    little_uint32_t headerSize;
    little_uint32_t headerCrc32;
    little_uint32_t reserved;
    little_uint64_t thisHeaderLba;
    little_uint64_t alternateHeaderLba;
    little_uint64_t firstUsableBlock;
    little_uint64_t lastUsableBlock;
    char guid[16];
    little_uint64_t partitionTableLba;
    little_uint32_t numPartitionEntries;
    little_uint32_t partitionEntrySize;
    little_uint32_t partitionTableCrc32;
};

struct GptPartitionEntry {
    char typeGuid[16];
    char partitionGuid[16];
    little_uint64_t startLba;
    little_uint64_t endLba;
    little_uint64_t attributes;
    char name[72];
};

void Partition::scanPartitions(const Reference<Vnode>& device,
        const char* deviceName, size_t sectorSize) {
    GptHeader gpt;
    if (device->pread(&gpt, sizeof(gpt), sectorSize, 0) == sizeof(gpt)) {
        if (memcmp(&gpt.signature, "EFI PART", 8) == 0) {
            size_t partitionsFound = 0;
            for (uint32_t i = 0; i < gpt.numPartitionEntries; i++) {
                uint64_t offset = gpt.partitionTableLba * sectorSize +
                        i * gpt.partitionEntrySize;
                GptPartitionEntry entry;
                if (device->pread(&entry, sizeof(entry), offset, 0) ==
                        sizeof(entry)) {
                    static const char empty[16] = {0};
                    if (memcmp(&entry.typeGuid, empty, 16) == 0) continue;

                    size_t size = (entry.endLba - entry.startLba) * sectorSize;
                    Reference<Partition> partition = xnew Partition(device,
                            entry.startLba * sectorSize, size);
                    partitionsFound++;

                    char name[32];
                    snprintf(name, sizeof(name), "%sp%zu", deviceName,
                            partitionsFound);
                    devFS.addDevice(name, partition);
                }
            }
        }
    }

    // TODO: Check for MBR
}

Partition::Partition(const Reference<Vnode>& device, off_t offset, size_t size)
        : Vnode(S_IFBLK | 0644, DevFS::dev),
        device(device) {
    partitionOffset = offset;
    stats.st_size = size;
    stats.st_blksize = device->stat().st_blksize;
}

bool Partition::isSeekable() {
    return true;
}

off_t Partition::lseek(off_t offset, int whence) {
    AutoLock lock(&mutex);
    off_t base;

    if (whence == SEEK_SET || whence == SEEK_CUR) {
        base = 0;
    } else if (whence == SEEK_END) {
        base = stats.st_size;
    } else {
        errno = EINVAL;
        return -1;
    }

    off_t result;
    if (__builtin_add_overflow(base, offset, &result) || result < 0 ||
            result > stats.st_size) {
        errno = EINVAL;
        return -1;
    }

    return result;
}

short Partition::poll() {
    return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
}

ssize_t Partition::pread(void* buffer, size_t size, off_t offset, int flags) {
    AutoLock lock(&mutex);

    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }

    if (offset > stats.st_size) return 0;

    if ((off_t) size > stats.st_size || stats.st_size - (off_t) size < offset) {
        size = stats.st_size - offset;
    }

    return device->pread(buffer, size, partitionOffset + offset, flags);
}

ssize_t Partition::pwrite(const void* buffer, size_t size, off_t offset,
        int flags) {
    AutoLock lock(&mutex);

    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }

    if (offset > stats.st_size) {
        errno = ENOSPC;
        return -1;
    }

    if ((off_t) size > stats.st_size || stats.st_size - (off_t) size < offset) {
        size = stats.st_size - offset;
    }

    return device->pwrite(buffer, size, partitionOffset + offset, flags);
}
