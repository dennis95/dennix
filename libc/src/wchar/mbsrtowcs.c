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

/* libc/src/wchar/mbsrtowcs.c
 * Convert a multi byte string to a wide character string.
 * (C95, called from C89)
 */

#define mbrtowc __mbrtowc
#include <stdint.h>
#include <wchar.h>

static mbstate_t staticPs;

size_t __mbsrtowcs(wchar_t* restrict wcs, const char** restrict s, size_t size,
        mbstate_t* restrict ps) {
    if (!ps) {
        ps = &staticPs;
    }

    for (size_t i = 0; !wcs || i < size; i++) {
        size_t bytes = mbrtowc(wcs ? wcs + i : NULL, *s, SIZE_MAX, ps);
        if (bytes == (size_t) -1) {
            return -1;
        } else if (bytes == 0) {
            *s = NULL;
            return i;
        } else {
            *s += bytes;
        }
    }

    return size;
}
__weak_alias(__mbsrtowcs, mbsrtowcs);
