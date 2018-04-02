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

/* sh/expand.c
 * Word expansion.
 */

#include <stdlib.h>

#include "expand.h"
#include "stringbuffer.h"

char* expandWord(char* token) {
    struct StringBuffer buffer;
    if (!initStringBuffer(&buffer)) return NULL;

    bool escaped = false;
    bool singleQuote = false;
    bool doubleQuote = false;

    while (*token) {
        char c = *token++;

        // TODO: Expand variables, command substitution, ...

        if (!escaped) {
            // Quote removal
            if (!singleQuote && c == '\\') {
                escaped = true;
                continue;
            } else if (!doubleQuote && c == '\'') {
                singleQuote = !singleQuote;
                continue;
            } else if (!singleQuote && c == '"') {
                doubleQuote = !doubleQuote;
                continue;
            }
        }

        escaped = false;

        if (!appendToStringBuffer(&buffer, c)) {
            free(buffer.buffer);
            return NULL;
        }
    }

    return finishStringBuffer(&buffer);
}
