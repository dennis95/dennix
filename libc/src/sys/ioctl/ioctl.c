/* Copyright (c) 2018 Dennis Wölfing
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

/* libc/src/sys/ioctl/ioctl.c
 * Legacy device control.
 */

#include <devctl.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/ioctl.h>

#define alloca(x) __builtin_alloca(x)

int ioctl(int fd, int command, ...) {
    size_t size;
    void* data;
    va_list ap;
    va_start(ap, command);

    switch (_IOCTL_TYPE(command)) {
    case _IOCTL_VOID:
        size = 0;
        data = NULL;
        break;
    case _IOCTL_INT:
        size = sizeof(int);
        data = alloca(size);
        *((int*) data) = va_arg(ap, int);
        break;
    case _IOCTL_LONG:
        size = sizeof(long);
        data = alloca(size);
        *((long*) data) = va_arg(ap, long);
        break;
    case _IOCTL_PTR:
        size = 0;
        data = va_arg(ap, void*);
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    va_end(ap);
    int info;
    errno = posix_devctl(fd, command, data, size, &info);
    return info;
}
