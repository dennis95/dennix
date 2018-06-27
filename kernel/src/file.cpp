/* Copyright (c) 2016, 2017, 2018 Dennis Wölfing
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

/* kernel/src/file.cpp
 * File Vnode.
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <dennix/seek.h>
#include <dennix/stat.h>
#include <dennix/kernel/file.h>

FileVnode::FileVnode(const void* data, size_t size, mode_t mode, dev_t dev)
        : Vnode(S_IFREG | mode, dev) {
    this->data = (char*) malloc(size);
    memcpy(this->data, data, size);
    stats.st_size = size;
}

FileVnode::~FileVnode() {
    free(data);
}

int FileVnode::ftruncate(off_t length) {
    if (length < 0) {
        errno = EINVAL;
        return -1;
    }
    if (length > SIZE_MAX) {
        errno = EFBIG;
        return -1;
    }

    AutoLock lock(&mutex);
    void* newData = realloc(data, (size_t) length);
    if (!newData) {
        errno = ENOSPC;
        return -1;
    }
    data = (char*) newData;

    if (length > stats.st_size) {
        memset(data + stats.st_size, '\0', length - stats.st_size);
    }

    stats.st_size = length;
    updateTimestamps(false, true, true);
    return 0;
}

bool FileVnode::isSeekable() {
    return true;
}

off_t FileVnode::lseek(off_t offset, int whence) {
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
    if (__builtin_add_overflow(base, offset, &result) || result < 0) {
        errno = EINVAL;
        return -1;
    }

    return result;
}

ssize_t FileVnode::pread(void* buffer, size_t size, off_t offset) {
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    if (size == 0) return 0;

    AutoLock lock(&mutex);
    char* buf = (char*) buffer;

    for (size_t i = 0; i < size; i++) {
        if (offset + i >= stats.st_size) return i;
        buf[i] = data[offset + i];
    }

    updateTimestamps(true, false, false);
    return size;
}

ssize_t FileVnode::pwrite(const void* buffer, size_t size, off_t offset) {
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }

    if (size == 0) return 0;

    AutoLock lock(&mutex);
    off_t newSize;
    if (__builtin_add_overflow(offset, size, &newSize)) {
        errno = ENOSPC;
        return -1;
    }

    if (newSize > SIZE_MAX) {
        errno = EFBIG;
        return -1;
    }

    if (newSize > stats.st_size) {
        void* newData = realloc(data, (size_t) newSize);
        if (!newData) {
            errno = ENOSPC;
            return -1;
        }
        data = (char*) newData;

        if (offset > stats.st_size) {
            // When writing after the old EOF, fill the gap with zeros.
            memset(data + stats.st_size, '\0', offset - stats.st_size);
        }

        stats.st_size = newSize;
    }

    memcpy(data + offset, buffer, size);
    updateTimestamps(false, true, true);
    return size;
}
