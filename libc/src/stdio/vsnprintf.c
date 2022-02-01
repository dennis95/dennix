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

/* libc/src/stdio/vsnprintf.c
 * Print formatted output. (C99, called from C89)
 */

#define vcbprintf __vcbprintf
#include <errno.h>
#include <limits.h>
#include <stdio.h>

struct context {
    char* buffer;
    size_t bufferSize;
    size_t index;
};

static size_t callback(void* arg, const char* s, size_t length) {
    struct context* context = arg;
    if (context->bufferSize == 0) return length;

    for (size_t i = 0; i < length; i++) {
        if (context->index == context->bufferSize - 1) return length;
        context->buffer[context->index++] = s[i];
    }

    return length;
}

int __vsnprintf(char* restrict s, size_t n, const char* restrict format,
        va_list ap) {
    if (n > INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    struct context context;
    context.buffer = s;
    context.index = 0;
    context.bufferSize = n;

    int result = vcbprintf(&context, callback, format, ap);
    if (n != 0) {
        s[context.index] = '\0';
    }
    return result;
}
__weak_alias(__vsnprintf, vsnprintf);
