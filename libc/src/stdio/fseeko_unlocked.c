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

/* libc/src/stdio/fseeko_unlocked.c
 * Set file position.
 */

#include "FILE.h"
#include <errno.h>

int fseeko_unlocked(FILE* file, off_t offset, int whence) {
    if (fileWasWritten(file) && fflush_unlocked(file) == EOF) {
        return -1;
    }

    if (whence == SEEK_CUR) {
        if (__builtin_sub_overflow(offset, file->readEnd - file->readPosition,
                &offset)) {
            errno = EOVERFLOW;
            return -1;
        }
    }

    if (file->seek(file, offset, whence) < 0) return -1;

    file->flags &= ~FILE_FLAG_EOF;
    file->readPosition = UNGET_BYTES;
    file->readEnd = UNGET_BYTES;
    return 0;
}
