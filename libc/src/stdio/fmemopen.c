/* Copyright (c) 2019, 2020, 2022 Dennis Wölfing
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

/* libc/src/stdio/fmemopen.c
 * Open a memory buffer stream. (POSIX2008)
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "FILE.h"

struct memfile {
    FILE file;
    unsigned char ungetcBuffer[UNGET_BYTES];
    unsigned char* buffer;
    size_t offset;
    size_t currentSize;
    size_t maxSize;
    bool append;
    bool binary;
    bool update;

    // When we need to allocate the buffer for an update stream we put the
    // buffer at the end of the struct so that fclose will free the buffer.
    unsigned char allocatedBuffer[];
};

static size_t fmemopen_read(FILE* file, unsigned char* p, size_t size);
static size_t fmemopen_write(FILE* file, const unsigned char* p, size_t size);
static off_t fmemopen_seek(FILE* file, off_t offset, int whence);

FILE* fmemopen(void* restrict buffer, size_t size, const char* restrict mode) {
    int flags = __fmodeflags(mode);
    if (flags == -1) return NULL;

    if (!buffer) {
        if ((flags & O_RDWR) != O_RDWR) {
            errno = EINVAL;
            return NULL;
        }
    } else if (flags & O_TRUNC && flags & O_RDONLY) {
        // For whatever reason fmemopen is supposed to truncate the buffer for
        // "w+" but not for "w".
        *(unsigned char*) buffer = '\0';
    }

    bool binary = strchr(mode, 'b');

    size_t initialSize;
    if (binary || !(flags & O_CREAT)) {
        initialSize = size;
    } else if (flags & O_APPEND && buffer) {
        initialSize = strnlen(buffer, size);
    } else {
        initialSize = 0;
    }

    off_t initialOffset = (flags & O_APPEND) ? initialSize : 0;

    size_t allocSize;
    if (__builtin_add_overflow(sizeof(struct memfile), buffer ? 0 : size,
            &allocSize)) {
        errno = ENOMEM;
        return NULL;
    }

    struct memfile* memfile = malloc(allocSize);
    if (!memfile) return NULL;
    FILE* file = &memfile->file;

    file->fd = -1;
    file->flags = FILE_FLAG_USER_BUFFER;
    file->buffer = memfile->ungetcBuffer;
    file->bufferSize = UNGET_BYTES;
    file->readPosition = UNGET_BYTES;
    file->readEnd = UNGET_BYTES;
    file->writePosition = 0;
    file->mutex = (__mutex_t) _MUTEX_INIT(_MUTEX_RECURSIVE);

    file->read = fmemopen_read;
    file->write = fmemopen_write;
    file->seek = fmemopen_seek;

    if (!buffer) {
        buffer = memfile->allocatedBuffer;
        memset(buffer, 0, size);
    }

    memfile->buffer = buffer;
    memfile->offset = initialOffset;
    memfile->currentSize = initialSize;
    memfile->maxSize = size;
    memfile->append = flags & O_APPEND;
    memfile->binary = binary;
    memfile->update = (flags & O_RDWR) == O_RDWR;

    pthread_mutex_lock(&__fileListMutex);
    file->prev = NULL;
    file->next = __firstFile;
    if (file->next) {
        file->next->prev = file;
    }
    __firstFile = file;
    pthread_mutex_unlock(&__fileListMutex);

    return file;
}

static size_t fmemopen_read(FILE* file, unsigned char* p, size_t size) {
    struct memfile* memfile = (struct memfile*) file;
    if (memfile->offset >= memfile->currentSize) {
        file->flags |= FILE_FLAG_EOF;
        return 0;
    }

    size_t bytesAvailable = memfile->currentSize - memfile->offset;
    size_t bytesRead = bytesAvailable >= size ? size : bytesAvailable;
    memcpy(p, memfile->buffer + memfile->offset, bytesRead);
    memfile->offset += bytesRead;
    return bytesRead;
}

static size_t fmemopen_write(FILE* file, const unsigned char* p, size_t size) {
    struct memfile* memfile = (struct memfile*) file;
    size_t offset = memfile->append ? memfile->currentSize : memfile->offset;
    size_t spaceLeft = memfile->maxSize - offset;
    size_t bytesWritten = spaceLeft >= size ? size : spaceLeft;
    memcpy(memfile->buffer + offset, p, bytesWritten);

    memfile->offset = offset + bytesWritten;
    if (memfile->offset > memfile->currentSize) {
        memfile->currentSize = memfile->offset;
        if (!memfile->binary) {
            // POSIX is unclear about when and how a NUL byte is supposed to be
            // written to the buffer.
            // See http://austingroupbugs.net/view.php?id=657
            if (memfile->currentSize < memfile->maxSize) {
                memfile->buffer[memfile->currentSize] = '\0';
            } else if (!memfile->update) {
                memfile->buffer[memfile->maxSize - 1] = '\0';
            }
        }
    }

    if (bytesWritten < size) {
        file->flags |= FILE_FLAG_ERROR;
        errno = ENOSPC;
    }
    return bytesWritten;
}

static off_t fmemopen_seek(FILE* file, off_t offset, int whence) {
    struct memfile* memfile = (struct memfile*) file;

    off_t base;
    if (whence == SEEK_SET) {
        base = 0;
    } else if (whence == SEEK_CUR) {
        base = memfile->offset;
    } else if (whence == SEEK_END) {
        base = memfile->currentSize;
    } else {
        errno = EINVAL;
        return -1;
    }

    off_t newOffset;
    if (__builtin_add_overflow(base, offset, &newOffset)) {
        errno = EOVERFLOW;
        return -1;
    }
    if (newOffset < 0 || newOffset > (off_t) memfile->maxSize) {
        errno = EINVAL;
        return -1;
    }

    memfile->offset = newOffset;
    return newOffset;
}
