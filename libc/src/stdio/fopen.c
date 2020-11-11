/* Copyright (c) 2016, 2019, 2020 Dennis Wölfing
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

/* libc/src/stdio/fopen.c
 * Opens a file.
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

FILE* fopen(const char* restrict path, const char* restrict mode) {
    int flags = __fmodeflags(mode);
    if (flags == -1) return NULL;
    // Ignore x when not creating a file.
    if (!(flags & O_CREAT)) flags &= ~O_EXCL;

    int fd = open(path, flags,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fd == -1) return NULL;
    FILE* result = fdopen(fd, mode);
    if (!result) {
        close(fd);
    }
    return result;
}
