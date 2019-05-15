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

/* libc/src/stdio/__fmodeflags.c
 * Get file open flags from mode.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

int __fmodeflags(const char* mode) {
    int result = 0;

    while (*mode) {
        switch(*mode++) {
        case 'r': result |= O_RDONLY; break;
        case 'w': result |= O_WRONLY | O_CREAT | O_TRUNC; break;
        case 'a': result |= O_WRONLY | O_APPEND | O_CREAT; break;
        case '+': result |= O_RDWR; break;
        case 'e': result |= O_CLOEXEC; break;
        case 'x': result |= O_EXCL; break;
        case 'b': break;
        default:
            errno = EINVAL;
            return -1;
        }
    }

    return result;
}
