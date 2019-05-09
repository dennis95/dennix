/* Copyright (c) 2017, 2018, 2019 Dennis Wölfing
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

/* libc/src/stdio/FILE.h
 * FILE structure.
 */

#ifndef FILE_H
#define FILE_H

#include <stdbool.h>
#include <stdio.h>

struct __FILE {
    int fd;
    int flags;
    unsigned char* buffer;
    size_t bufferSize;
    size_t readPosition;
    size_t readEnd;
    size_t writePosition;
    FILE* prev;
    FILE* next;
};

#define FILE_FLAG_EOF (1 << 0)
#define FILE_FLAG_ERROR (1 << 1)
#define FILE_FLAG_BUFFERED (1 << 2)
#define FILE_FLAG_LINEBUFFER (1 << 3)
#define FILE_FLAG_USER_BUFFER (1 << 4)

#define UNGET_BYTES 8

extern FILE* __firstFile;

static inline bool fileWasRead(FILE* file) {
    return file->readPosition != file->readEnd;
}

static inline bool fileWasWritten(FILE* file) {
    return file->writePosition != 0;
}

size_t __file_write(FILE* file, const unsigned char* p, size_t size);

#endif
