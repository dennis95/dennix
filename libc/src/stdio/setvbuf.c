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

/* libc/src/stdio/setvbuf.c
 * Set file buffer.
 */

#include <stdlib.h>
#include "FILE.h"

int setvbuf(FILE* restrict file, char* restrict buffer, int type, size_t size) {
    if (type == _IOFBF) {
        file->flags |= FILE_FLAG_BUFFERED;
        file->flags &= ~FILE_FLAG_LINEBUFFER;
    } else if (type == _IOLBF) {
        file->flags |= FILE_FLAG_BUFFERED | FILE_FLAG_LINEBUFFER;
    } else if (type == _IONBF) {
        file->flags &= ~(FILE_FLAG_BUFFERED | FILE_FLAG_LINEBUFFER);
    } else {
        return -1;
    }

    if (buffer && size >= UNGET_BYTES) {
        if (!(file->flags & FILE_FLAG_USER_BUFFER)) {
            free(file->buffer);
        }

        file->buffer = (unsigned char*) buffer;
        file->bufferSize = size;
        file->flags |= FILE_FLAG_USER_BUFFER;
    }

    return 0;
}
