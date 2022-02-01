/* Copyright (c) 2016, 2017, 2019, 2022 Dennis Wölfing
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

/* libc/src/stdio/fwrite_unlocked.c
 * Writes data to a file. (called from C89)
 */

#include <string.h>
#include "FILE.h"

size_t __fwrite_unlocked(const void* restrict ptr, size_t size, size_t count,
        FILE* restrict file) {
    size_t bytes = size * count;
    if (!bytes) return 0;

    const unsigned char* p = (const unsigned char*) ptr;

    if (!(file->flags & FILE_FLAG_BUFFERED)) {
        return file->write(file, p, bytes) / size;
    }

    if (bytes > file->bufferSize - file->writePosition) {
        if (file->write(file, file->buffer, file->writePosition) <
                file->writePosition) {
            file->writePosition = 0;
            return 0;
        }
        file->writePosition = 0;
    }
    if (bytes > file->bufferSize) {
        return file->write(file, p, bytes) / size;
    }

    size_t written = 0;
    if (file->flags & FILE_FLAG_LINEBUFFER) {
        size_t i = bytes;
        while (i > 0 && p[i - 1] != '\n') {
            i--;
        }
        if (i > 0) {
            memcpy(file->buffer + file->writePosition, p, i);
            written = file->write(file, file->buffer, file->writePosition + i);
            if (written < file->writePosition + i) {
                written = written <= file->writePosition ? 0 :
                        written - file->writePosition;
                file->writePosition = 0;
                return written / size;
            }
            written = i;
            file->writePosition = 0;
        }
    }

    memcpy(file->buffer + file->writePosition, p + written, bytes - written);
    file->writePosition += bytes - written;
    return count;
}
__weak_alias(__fwrite_unlocked, fwrite_unlocked);
