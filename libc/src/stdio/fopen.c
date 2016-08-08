/* Copyright (c) 2016, Dennis Wölfing
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

FILE* fopen(const char* restrict path, const char* restrict mode) {
    int flags;

    if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) {
        flags = O_RDONLY;
    } else if (strcmp(mode, "w") == 0 || strcmp(mode, "wb") == 0) {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (strcmp(mode, "a") == 0 || strcmp(mode, "ab") == 0) {
        flags = O_WRONLY | O_CREAT | O_APPEND;
    } else if (strcmp(mode, "r+") == 0 || strcmp(mode, "rb+") == 0 ||
            strcmp(mode, "r+b") == 0) {
        flags = O_RDWR;
    } else if (strcmp(mode, "w+") == 0 || strcmp(mode, "wb+") == 0 ||
            strcmp(mode, "w+b") == 0) {
        flags = O_RDWR | O_CREAT | O_TRUNC;
    } else if (strcmp(mode, "a+") == 0 || strcmp(mode, "ab+") == 0 ||
            strcmp(mode, "a+b") == 0) {
        flags = O_RDWR | O_CREAT | O_APPEND;
    } else {
        errno = EINVAL;
        return NULL;
    }

    int fd = open(path, flags,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fd == -1) return NULL;
    return fdopen(fd, mode);
}
