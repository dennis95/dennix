/* Copyright (c) 2019, 2022 Dennis Wölfing
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

/* libc/src/wchar/wcrtomb.c
 * Convert a wide character to a multibyte character. (C95, called from C89)
 */

#include <errno.h>
#include <wchar.h>

size_t __wcrtomb(char* restrict s, wchar_t wc, mbstate_t* restrict ps) {
    (void) ps;
    if (!s) return 1;

    if ((wc >= 0xD800 && wc <= 0xDFFF) || wc > 0x10FFFF) {
        errno = EILSEQ;
        return -1;
    }

    if (wc <= 0x7F) {
        s[0] = wc;
        return 1;
    } else if (wc <= 0x7FF) {
        s[0] = 0b11000000 | ((wc >> 6) & 0b00011111);
        s[1] = 0b10000000 | (wc & 0b00111111);
        return 2;
    } else if (wc <= 0xFFFF) {
        s[0] = 0b11100000 | ((wc >> 12) & 0b00001111);
        s[1] = 0b10000000 | ((wc >> 6) & 0b00111111);
        s[2] = 0b10000000 | (wc & 0b00111111);
        return 3;
    } else {
        s[0] = 0b11110000 | ((wc >> 18) & 0b00000111);
        s[1] = 0b10000000 | ((wc >> 12) & 0b00111111);
        s[2] = 0b10000000 | ((wc >> 6) & 0b00111111);
        s[3] = 0b10000000 | (wc & 0b00111111);
        return 4;
    }
}
__weak_alias(__wcrtomb, wcrtomb);
