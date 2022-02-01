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

/* libc/src/wchar/mbrtowc.c
 * Convert a multi byte character to a wide character. (C95, called from C89)
 */

#include <errno.h>
#include <wchar.h>

static mbstate_t staticPs;

size_t __mbrtowc(wchar_t* restrict wc, const char* restrict s, size_t size,
        mbstate_t* restrict ps) {
    if (!s) {
        wc = NULL;
        s = "";
        size = 1;
    }

    if (!ps) {
        ps = &staticPs;
    }

    for (size_t i = 0; i < size; i++) {
        unsigned char c = (unsigned char) s[i];
        if (ps->_state == 0) {
            if ((c & 0b10000000) == 0b00000000) { // single byte character
                ps->_wc = (wchar_t) c;
            } else if ((c & 0b11100000) == 0b11000000) { // 2 byte character
                ps->_wc = ((wchar_t) c & 0b00011111) << 6;
                if (ps->_wc < 0x80) {
                    errno = EILSEQ;
                    return -1;
                }
                ps->_state = 1;
            } else if ((c & 0b11110000) == 0b11100000) { // 3 byte character
                ps->_wc = ((wchar_t) c & 0b00001111) << 12;
                ps->_state = 2;
            } else if ((c & 0b11111000) == 0b11110000) { // 4 byte character
                ps->_wc = ((wchar_t) c & 0b00000111) << 18;
                ps->_state = 3;
            } else {
                errno = EILSEQ;
                return -1;
            }
        } else if ((c & 0b11000000) != 0b10000000) {
            errno = EILSEQ;
            return -1;
        } else if (ps->_state == 1) {
            ps->_wc |= (wchar_t) c & 0b00111111;
            ps->_state = 0;
        } else if (ps->_state == 2) {
            ps->_wc |= ((wchar_t) c & 0b00111111) << 6;
            if (ps->_wc < 0x800 || (ps->_wc >= 0xD800 && ps->_wc <= 0xDFFF)) {
                errno = EILSEQ;
                return -1;
            }
            ps->_state = 1;
        } else if (ps->_state == 3) {
            ps->_wc |= ((wchar_t) c & 0b00111111) << 12;
            if (ps->_wc < 0x10000 || ps->_wc > 0x10FFFF) {
                errno = EILSEQ;
                return -1;
            }
            ps->_state = 4;
        } else if (ps->_state == 4) {
            ps->_wc |= ((wchar_t) c & 0b00111111) << 6;
            ps->_state = 1;
        } else {
            errno = EINVAL;
            return -1;
        }

        if (ps->_state == 0) {
            // A character has been completed.
            if (wc) {
                *wc = ps->_wc;
            }
            return ps->_wc ? i + 1 : 0;
        }
    }

    return -2;
}
__weak_alias(__mbrtowc, mbrtowc);
