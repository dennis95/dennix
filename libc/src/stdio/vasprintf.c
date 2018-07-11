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

/* libc/src/stdio/vasprintf.c
 * Print formatted output.
 */

#include <stdio.h>
#include <stdlib.h>

struct context {
    char* buffer;
    size_t allocated;
    size_t used;
};

static size_t callback(void* arg, const char* s, size_t length) {
    struct context* context = arg;
    if (context->used + length >= context->allocated) {
        size_t newSize = context->allocated * 2;
        if (newSize < context->used + length + 1) {
            newSize = context->used + length + 1;
        }
        char* newBuffer = realloc(context->buffer, newSize);
        if (!newBuffer) return 0;
        context->buffer = newBuffer;
        context->allocated = newSize;
    }

    for (size_t i = 0; i < length; i++) {
        context->buffer[context->used++] = s[i];
    }

    return length;
}

int vasprintf(char** restrict strp, const char* restrict format, va_list ap) {
    struct context context;
    context.buffer = malloc(80);
    if (!context.buffer) return -1;
    context.allocated = 80;
    context.used = 0;

    int result = vcbprintf(&context, callback, format, ap);
    if (result < 0) {
        free(context.buffer);
        return -1;
    }

    context.buffer[context.used] = '\0';
    *strp = context.buffer;
    return result;
}
