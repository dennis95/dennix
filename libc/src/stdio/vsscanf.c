/* Copyright (c) 2018, 2022 Dennis Wölfing
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

/* libc/src/stdio/vsscanf.c
 * Scan formatted input. (C99, called from C89)
 */

#define vcbscanf __vcbscanf
#include <stdio.h>

static int get(void* arg) {
    const unsigned char** s = arg;
    unsigned char c = **s;
    if (!c) return EOF;
    (*s)++;
    return c;
}

static int unget(int c, void* arg) {
    if (c == EOF) return EOF;
    const unsigned char** s = arg;
    (*s)--;
    return c;
}

int __vsscanf(const char* restrict s, const char* restrict format, va_list ap) {
    return vcbscanf((void*) &s, get, unget, format, ap);
}
__weak_alias(__vsscanf, vsscanf);
