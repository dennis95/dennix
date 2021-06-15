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

/* libc/src/wctype/wctype.c
 * Wide character types.
 */

#include <ctype.h>
#include <wctype.h>

int iswalnum(wint_t c) {
    return c <= 255 ? isalnum(c) : 0;
}

int iswalpha(wint_t c) {
    return c <= 255 ? isalpha(c) : 0;
}

int iswblank(wint_t c) {
    return c <= 255 ? isblank(c) : 0;
}

int iswcntrl(wint_t c) {
    return c <= 255 ? iscntrl(c) : 0;
}

int iswdigit(wint_t c) {
    return c <= 255 ? isdigit(c) : 0;
}

int iswgraph(wint_t c) {
    return c <= 255 ? isgraph(c) : 0;
}

int iswlower(wint_t c) {
    return c <= 255 ? islower(c) : 0;
}

int iswprint(wint_t c) {
    return c <= 255 ? isprint(c) : 0;
}

int iswpunct(wint_t c) {
    return c <= 255 ? ispunct(c) : 0;
}

int iswspace(wint_t c) {
    return c <= 255 ? isspace(c) : 0;
}

int iswupper(wint_t c) {
    return c <= 255 ? isupper(c) : 0;
}

int iswxdigit(wint_t c) {
    return c <= 255 ? isxdigit(c) : 0;
}

wint_t towlower(wint_t c) {
    return c <= 255 ? (wint_t) tolower(c) : c;
}

wint_t towupper(wint_t c) {
    return c <= 255 ? (wint_t) toupper(c) : c;
}
