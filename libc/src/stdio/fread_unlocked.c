/* Copyright (c) 2018, 2019 Dennis Wölfing
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

/* libc/src/stdio/fread_unlocked.c
 * Read data from file.
 */

#include <string.h>
#include "FILE.h"

size_t fread_unlocked(void* restrict ptr, size_t size, size_t count,
        FILE* restrict file) {
    size_t bytes = size * count;
    if (!bytes) return 0;
    if (file->flags & FILE_FLAG_EOF) return 0;

    unsigned char* p = (unsigned char*) ptr;

    size_t bufferFilled = file->readEnd - file->readPosition;
    if (bufferFilled <= bytes) {
        memcpy(p, file->buffer + file->readPosition, bufferFilled);
        file->readPosition = UNGET_BYTES;
        file->readEnd = UNGET_BYTES;
    } else {
        memcpy(p, file->buffer + file->readPosition, bytes);
        file->readPosition += bytes;
        return count;
    }

    size_t bytesRemaining = bytes - bufferFilled;
    if (bytesRemaining == 0) return count;
    if (bytesRemaining >= file->bufferSize - file->readEnd ||
            !(file->flags & FILE_FLAG_BUFFERED)) {
        size_t bytesRead = file->read(file, p + bufferFilled, bytesRemaining);
        return (bufferFilled + bytesRead) / size;
    }

    size_t bytesRead = file->read(file, file->buffer + file->readEnd,
            file->bufferSize - file->readEnd);
    file->readEnd += bytesRead;

    size_t toCopy = bytesRemaining > bytesRead ? bytesRead : bytesRemaining;
    memcpy(p + bufferFilled, file->buffer + file->readPosition, toCopy);
    file->readPosition += toCopy;
    return (bufferFilled + toCopy) / size;
}
