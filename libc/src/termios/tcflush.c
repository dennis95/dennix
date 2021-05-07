/* Copyright (c) 2021 Dennis Wölfing
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

/* libc/src/termios/tcflush.c
 * Flush terminal buffer.
 */

#include <devctl.h>
#include <errno.h>
#include <termios.h>

int tcflush(int fd, int selector) {
    if (selector != TCIFLUSH && selector != TCIOFLUSH && selector != TCOFLUSH) {
        errno = EINVAL;
        return -1;
    }

    int info;
    errno = posix_devctl(fd, TCFLSH, &selector, sizeof(selector), &info);
    if (errno == EINVAL) errno = ENOTTY;
    return info;
}
