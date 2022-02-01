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

/* libc/src/stdio/vsprintf.c
 * Print formatted output. (C89)
 */

#define vcbprintf __vcbprintf
#include <stdio.h>

static size_t callback(void* arg, const char* s, size_t length) {
    char** buffer = arg;
    for (size_t i = 0; i < length; i++) {
        **buffer = s[i];
        (*buffer)++;
    }

    return length;
}

int vsprintf(char* restrict s, const char* restrict format, va_list ap) {
    int result = vcbprintf((void*) &s, callback, format, ap);
    if (result >= 0) {
        *s = '\0';
    }
    return result;
}
