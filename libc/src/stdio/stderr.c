/* Copyright (c) 2016, 2017, 2018, 2019, 2022 Dennis Wölfing
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

/* libc/src/stdio/stderr.c
 * Standard error.
 */

#include "FILE.h"

static unsigned char buffer[UNGET_BYTES];
static FILE __stderr = {
    .fd = 2,
    .flags = FILE_FLAG_USER_BUFFER,
    .buffer = buffer,
    .bufferSize = sizeof(buffer),
    .readPosition = UNGET_BYTES,
    .readEnd = UNGET_BYTES,
    .mutex = _MUTEX_INIT(_MUTEX_RECURSIVE),
    .read = __file_read,
    .write = __file_write,
    .seek = __file_seek,
};

FILE* stderr = &__stderr;
