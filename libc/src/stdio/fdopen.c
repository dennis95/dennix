/* Copyright (c) 2016, 2017, 2019, 2020, 2022 Dennis Wölfing
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

/* libc/src/stdio/fdopen.c
 * Associates a file with a file descriptor. (POSIX2008, called from C89)
 */

#define fcntl __fcntl
#define isatty __isatty
#define pthread_mutex_lock __mutex_lock
#define pthread_mutex_unlock __mutex_unlock
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "FILE.h"

FILE* __fdopen(int fd, const char* mode) {
    int flags = __fmodeflags(mode);
    if (flags == -1) return NULL;

    FILE* file = malloc(sizeof(FILE));
    if (!file) return NULL;

    file->fd = fd;
    file->flags = FILE_FLAG_BUFFERED;

    file->buffer = malloc(BUFSIZ);
    if (!file->buffer) {
        free(file);
        return NULL;
    }
    file->bufferSize = BUFSIZ;

    file->readPosition = UNGET_BYTES;
    file->readEnd = UNGET_BYTES;
    file->writePosition = 0;

    file->mutex = (__mutex_t) _MUTEX_INIT(_MUTEX_RECURSIVE);
    file->read = __file_read;
    file->write = __file_write;
    file->seek = __file_seek;

    if (isatty(fd)) {
        file->flags |= FILE_FLAG_LINEBUFFER;
    }
    if (flags & O_RDONLY) {
        file->flags |= FILE_FLAG_READABLE;
    }
    if (flags & O_WRONLY) {
        file->flags |= FILE_FLAG_WRITABLE;
    }

    if (flags & O_CLOEXEC) {
        int fdflags = fcntl(fd, F_GETFD);
        if (!(fdflags & FD_CLOEXEC)) {
            fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC);
        }
    }

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
__weak_alias(__fdopen, fdopen);
