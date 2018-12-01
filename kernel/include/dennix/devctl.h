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

/* kernel/include/dennix/devctl.h
 * Device Control.
 */

#ifndef _DENNIX_DEVCTL_H
#define _DENNIX_DEVCTL_H

/* For the posix_devctl() function, data is always passed as pointer and size.
   The ioctl() funtion however can accept different types. Therefore each devctl
   number needs to encode information about the type used by ioctl(). */
#define _DEVCTL(type, number) (((type) << 29) | (number))
#define _IOCTL_TYPE(devctl) (((devctl) >> 29) & 7)

#define _IOCTL_VOID 0
#define _IOCTL_INT 1
#define _IOCTL_LONG 2
#define _IOCTL_PTR 3

#endif
