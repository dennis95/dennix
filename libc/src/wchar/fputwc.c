/* Copyright (c) 2022 Dennis Wölfing
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

/* libc/src/wchar/fputwc.c
 * Write a wide character to a file. (C95)
 */

#define flockfile __flockfile
#define funlockfile __funlockfile
#define fwrite_unlocked __fwrite_unlocked
#include "../stdio/FILE.h"
#include <limits.h>
#include <wchar.h>

wint_t fputwc(wchar_t wc, FILE* file) {
    char buffer[MB_LEN_MAX];
    mbstate_t ps = {0};
    size_t result = wcrtomb(buffer, wc, &ps);

    flockfile(file);
    if (result <= MB_LEN_MAX) {
        if (fwrite_unlocked(buffer, result, 1, file) == 1) {
            funlockfile(file);
            return wc;
        }
    }

    file->flags |= FILE_FLAG_ERROR;
    funlockfile(file);
    return WEOF;
}
