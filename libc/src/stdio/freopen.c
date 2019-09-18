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

/* libc/src/stdio/freopen.c
 * Reopen a file.
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "FILE.h"

FILE* freopen(const char* restrict pathname, const char* restrict mode,
        FILE* restrict file) {
    flockfile(file);

    fflush_unlocked(file);
    if (pathname && file->fd != -1) {
        close(file->fd);
        file->fd = -1;
    }
    clearerr_unlocked(file);

    int flags = __fmodeflags(mode);
    if (flags == -1) {
        fclose(file);
        errno = EINVAL;
        return NULL;
    }

    file->readPosition = UNGET_BYTES;
    file->readEnd = UNGET_BYTES;
    file->writePosition = 0;

    if (pathname) {
        int fd = open(pathname, flags, 0666);
        if (fd < 0) {
            goto fail;
        }
        file->fd = fd;

        file->read = __file_read;
        file->write = __file_write;
        file->seek = __file_seek;

        if (isatty(fd)) {
            file->flags |= FILE_FLAG_LINEBUFFER;
        }
    } else {
        if (flags & O_CLOEXEC) {
            // Error checking is not needed, all error conditions also apply to
            // the fcntl call below.
            fcntl(file->fd, F_SETFD, FD_CLOEXEC);
        }
        if (fcntl(file->fd, F_SETFL, flags) == -1) goto fail;
        if (flags & O_TRUNC && ftruncate(file->fd, 0) < 0) goto fail;
        if (lseek(file->fd, 0, (flags & O_APPEND) ? SEEK_END : SEEK_SET) < 0) {
            goto fail;
        }
    }

    funlockfile(file);
    return file;

fail: {
        int error = errno;
        fclose(file);
        errno = error;
        return NULL;
    }
}
