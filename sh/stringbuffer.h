/* Copyright (c) 2018, 2019 Dennis Wölfing
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

/* sh/stringbuffer.h
 * String buffer.
 */

#ifndef STRINGBUFFER_H
#define STRINGBUFFER_H

#include <stdbool.h>
#include <stddef.h>

#include "sh.h"

struct StringBuffer {
    char* buffer;
    size_t used;
    size_t allocated;
};

NO_DISCARD bool initStringBuffer(struct StringBuffer* buffer);
NO_DISCARD bool appendToStringBuffer(struct StringBuffer* buffer, char c);
NO_DISCARD bool appendStringToStringBuffer(struct StringBuffer* buffer,
        const char* s);
char* finishStringBuffer(struct StringBuffer* buffer);

#endif
