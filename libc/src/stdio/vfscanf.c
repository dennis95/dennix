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

/* libc/src/stdio/vfscanf.c
 * Scan formatted input. (C99, called from C89)
 */

#define fgetc_unlocked __fgetc_unlocked
#define flockfile __flockfile
#define funlockfile __funlockfile
#define ungetc_unlocked __ungetc_unlocked
#define vcbprintf __vcbprintf
#include <stdio.h>

static int get(void* file) {
    return fgetc_unlocked((FILE*) file);
}

static int unget(int c, void* file) {
    return ungetc_unlocked(c, (FILE*) file);
}

int __vfscanf(FILE* restrict file, const char* restrict format, va_list ap) {
    flockfile(file);
    int result = vcbscanf(file, get, unget, format, ap);
    funlockfile(file);
    return result;
}
__weak_alias(__vfscanf, vfscanf);
