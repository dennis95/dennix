/* Copyright (c) 2021, 2022 Dennis Wölfing
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

/* libc/src/wctype/iswctype.c
 * Wide character classification. (C95)
 */

#define iswblank __iswblank
#include <string.h>
#include <wctype.h>

int iswctype(wint_t wc, wctype_t charclass) {
    if (!charclass) return 0;
    return charclass(wc);
}

wctype_t wctype(const char* charclass) {
    if (strcmp(charclass, "alnum") == 0) return iswalnum;
    if (strcmp(charclass, "alpha") == 0) return iswalpha;
    if (strcmp(charclass, "blank") == 0) return iswblank;
    if (strcmp(charclass, "cntrl") == 0) return iswcntrl;
    if (strcmp(charclass, "digit") == 0) return iswdigit;
    if (strcmp(charclass, "graph") == 0) return iswgraph;
    if (strcmp(charclass, "lower") == 0) return iswlower;
    if (strcmp(charclass, "print") == 0) return iswprint;
    if (strcmp(charclass, "punct") == 0) return iswpunct;
    if (strcmp(charclass, "space") == 0) return iswspace;
    if (strcmp(charclass, "upper") == 0) return iswupper;
    if (strcmp(charclass, "xdigit") == 0) return iswxdigit;
    return NULL;
}
