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

/* sh/stringbuffer.c
 * String buffer.
 */

#include <stdlib.h>

#include "stringbuffer.h"

bool initStringBuffer(struct StringBuffer* buffer) {
    buffer->buffer = malloc(80);
    if (!buffer->buffer) return false;
    buffer->used = 0;
    buffer->allocated = 80;
    return true;
}

bool appendToStringBuffer(struct StringBuffer* buffer, char c) {
    if (buffer->used + 1 >= buffer->allocated) {
        void* newBuffer = reallocarray(buffer->buffer, 2, buffer->allocated);
        if (!newBuffer) return false;
        buffer->buffer = newBuffer;
        buffer->allocated *= 2;
    }
    buffer->buffer[buffer->used++] = c;
    return true;
}

char* finishStringBuffer(struct StringBuffer* buffer) {
    buffer->buffer[buffer->used] = '\0';
    return buffer->buffer;
}
