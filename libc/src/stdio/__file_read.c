/* Copyright (c) 2019 Dennis Wölfing
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

/* libc/src/stdio/__file_read.c
 * Read data from a file.
 */

#include <unistd.h>
#include "FILE.h"

size_t __file_read(FILE* file, unsigned char* p, size_t size) {
    ssize_t result = read(file->fd, p, size);
    if (result == 0) {
        file->flags |= FILE_FLAG_EOF;
        return 0;
    } else if (result < 0) {
        file->flags |= FILE_FLAG_ERROR;
        return 0;
    } else {
        return result;
    }
}
