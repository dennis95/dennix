/* Copyright (c) 2016, 2021, 2022 Dennis Wölfing
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

/* libc/src/stdio/vfprintf.c
 * Print format. (C89)
 */

#define flockfile __flockfile
#define funlockfile __funlockfile
#define fwrite_unlocked __fwrite_unlocked
#include <stdarg.h>
#include <stdio.h>

static size_t callback(void* file, const char* s, size_t nBytes) {
    return fwrite_unlocked(s, 1, nBytes, (FILE*) file);
}

int vfprintf(FILE* restrict file, const char* restrict format, va_list ap) {
    flockfile(file);
    int result = vcbprintf(file, callback, format, ap);
    funlockfile(file);
    return result;
}
