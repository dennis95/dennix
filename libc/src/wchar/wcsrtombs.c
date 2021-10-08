/* Copyright (c) 2021 Dennis Wölfing
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

/* libc/src/wchar/wcsrtombs.c
 * Convert a wide character string to a multibyte character string.
 */

#include <limits.h>
#include <string.h>
#include <wchar.h>

static mbstate_t staticPs;

size_t wcsrtombs(char* restrict s, const wchar_t** restrict wcs, size_t size,
        mbstate_t* restrict ps) {
    if (!ps) {
        ps = &staticPs;
    }

    char buffer[MB_LEN_MAX];
    size_t length = 0;
    for (size_t i = 0; ; i++) {
        size_t bytes = wcrtomb(buffer, (*wcs)[i], ps);
        if (bytes == (size_t) -1) {
            if (s) *wcs += i;
            return -1;
        }

        if (s && length + bytes > size) {
            *wcs += i;
            return length;
        }

        if (s) {
            memcpy(s + length, buffer, bytes);
        }
        if ((*wcs)[i] == L'\0') {
            if (s) *wcs = NULL;
            return length;
        }

        length += bytes;
    }
}
