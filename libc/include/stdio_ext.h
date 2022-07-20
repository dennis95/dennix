/* Copyright (c) 2022 Dennis Wölfing
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

/* libc/include/stdio_ext.h
 * Access to FILE internals. Use of these functions is discouraged.
 */

#ifndef _STDIO_EXT_H
#define _STDIO_EXT_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FSETLOCKING_INTERNAL 0
#define FSETLOCKING_BYCALLER 1
#define FSETLOCKING_QUERY 2

size_t __fbufsize(FILE*);
int __flbf(FILE*);
void _flushlbf(void);
size_t __fpending(FILE*);
void __fpurge(FILE*);
int __freadable(FILE*);
int __freading(FILE*);
int __fsetlocking(FILE*, int);
int __fwritable(FILE*);
int __fwriting(FILE*);

#ifdef __cplusplus
}
#endif

#endif
