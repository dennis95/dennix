/* Copyright (c) 2020 Dennis Wölfing
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

/* kernel/include/dennix/oflags.h
 * File open flags.
 */

#ifndef _DENNIX_OFLAGS_H
#define _DENNIX_OFLAGS_H

#define O_EXEC (1 << 0)
#define O_RDONLY (1 << 1)
#define O_WRONLY (1 << 2)
#define O_RDWR (O_RDONLY | O_WRONLY)
#define O_SEARCH O_EXEC

#define O_ACCMODE (O_EXEC | O_RDONLY | O_WRONLY)

#define O_APPEND (1 << 3)
#define O_CLOEXEC (1 << 4)
#define O_CREAT (1 << 5)
#define O_DIRECTORY (1 << 6)
#define O_EXCL (1 << 7)
#define O_NOCTTY (1 << 8)
#define O_NOFOLLOW (1 << 9)
#define O_NONBLOCK (1 << 10)
#define O_SYNC (1 << 11)
#define O_TRUNC (1 << 12)
#define O_NOCLOBBER (1 << 13)
#define O_TTY_INIT 0

#endif
