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

/* libc/src/stdio/fflush_unlocked.c
 * Flush a file stream.
 */

#include <errno.h>
#include <unistd.h>
#include "FILE.h"

int fflush_unlocked(FILE* file) {
    if (fileWasRead(file)) {
        off_t offset = -(off_t) (file->readEnd - file->readPosition);
        if (file->readPosition < UNGET_BYTES) {
            file->readPosition = UNGET_BYTES;
        }

        if (lseek(file->fd, offset, SEEK_CUR) < 0) {
            if (errno == ESPIPE) {
                // We need to preserve the buffer as we would lose data
                // otherwise. Staying in read mode is not a problem because
                // applications must seek (not just flush) when writing after
                // reading.
                return 0;
            }
            file->flags |= FILE_FLAG_ERROR;
            return EOF;
        }
        file->readPosition = UNGET_BYTES;
        file->readEnd = UNGET_BYTES;
    }

    if (fileWasWritten(file)) {
        if (__file_write(file, file->buffer, file->writePosition) <
                file->writePosition) {
            file->writePosition = 0;
            return EOF;
        }
        file->writePosition = 0;
    }

    return 0;
}
