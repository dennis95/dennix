/* Copyright (c) 2022 Dennis Wölfing
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

/* libc/src/stdio/stdio_ext.c
 * Access to FILE internals. Use of these functions is discouraged.
 */

#include "FILE.h"
#include <stdio_ext.h>

size_t __fbufsize(FILE* file) {
    flockfile(file);
    size_t result = file->bufferSize;
    funlockfile(file);
    return result;
}

int __flbf(FILE* file) {
    flockfile(file);
    int result = file->flags & FILE_FLAG_LINEBUFFER;
    funlockfile(file);
    return result;
}

static void flushIfLineBuffered(FILE* file) {
    flockfile(file);
    if (file->flags & FILE_FLAG_LINEBUFFER) {
        fflush_unlocked(file);
    }
    funlockfile(file);
}

void _flushlbf(void) {
    flushIfLineBuffered(stdin);
    flushIfLineBuffered(stdout);
    flushIfLineBuffered(stderr);

    pthread_mutex_lock(&__fileListMutex);
    FILE* file = __firstFile;
    while (file) {
        flushIfLineBuffered(file);
        file = file->next;
    }
    pthread_mutex_unlock(&__fileListMutex);
}

size_t __fpending(FILE* file) {
    flockfile(file);
    size_t result = file->writePosition;
    funlockfile(file);
    return result;
}

void __fpurge(FILE* file) {
    flockfile(file);
    file->readPosition = UNGET_BYTES;
    file->readEnd = UNGET_BYTES;
    file->writePosition = 0;
    funlockfile(file);
}

int __freadable(FILE* file) {
    flockfile(file);
    int result = file->flags & FILE_FLAG_READABLE;
    funlockfile(file);
    return result;
}

int __freading(FILE* file) {
    flockfile(file);
    int result = !(file->flags & FILE_FLAG_WRITABLE) ||
            file->readPosition < file->readEnd;
    funlockfile(file);
    return result;
}

int __fsetlocking(FILE* file, int locking) {
    // We do not allow the application to disable internal locking.
    (void) file; (void) locking;
    return FSETLOCKING_INTERNAL;
}

int __fwritable(FILE* file) {
    flockfile(file);
    int result = file->flags & FILE_FLAG_WRITABLE;
    funlockfile(file);
    return result;
}

int __fwriting(FILE* file) {
    flockfile(file);
    int result = !(file->flags & FILE_FLAG_READABLE) ||
            file->writePosition != 0;
    funlockfile(file);
    return result;
}
