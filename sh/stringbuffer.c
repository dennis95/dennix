/* Copyright (c) 2018, 2019, 2020, 2022 Dennis Wölfing
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

/* sh/stringbuffer.c
 * String buffer.
 */

#include <err.h>
#include <stdlib.h>

#include "stringbuffer.h"

void initStringBuffer(struct StringBuffer* buffer) {
    buffer->buffer = malloc(80);
    if (!buffer->buffer) err(1, "malloc");
    buffer->used = 0;
    buffer->allocated = 80;
}

void appendToStringBuffer(struct StringBuffer* buffer, char c) {
    if (buffer->used + 1 >= buffer->allocated) {
        void* newBuffer = reallocarray(buffer->buffer, 2, buffer->allocated);
        if (!newBuffer) err(1, "realloc");
        buffer->buffer = newBuffer;
        buffer->allocated *= 2;
    }
    buffer->buffer[buffer->used++] = c;
}

void appendBytesToStringBuffer(struct StringBuffer* buffer, const char* s,
        size_t length) {
    for (size_t i = 0; i < length; i++) {
        appendToStringBuffer(buffer, s[i]);
    }
}

void appendStringToStringBuffer(struct StringBuffer* buffer, const char* s) {
    while (*s) {
        appendToStringBuffer(buffer, *s);
        s++;
    }
}

char* finishStringBuffer(struct StringBuffer* buffer) {
    buffer->buffer[buffer->used] = '\0';
    return buffer->buffer;
}
