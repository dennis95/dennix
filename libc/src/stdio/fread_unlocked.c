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

/* libc/src/stdio/fread_unlocked.c
 * Read data from file.
 */

#include <stdio.h>

size_t fread_unlocked(void* restrict ptr, size_t size, size_t count,
        FILE* restrict file) {
    if (size == 0 || count == 0) return 0;

    unsigned char* p = (unsigned char*) ptr;
    size_t i;
    for (i = 0; i < count; i++) {
        for (size_t j = 0; j < size; j++) {
            int c = fgetc_unlocked(file);
            if (c == EOF) return i;
            p[i * size + j] = (unsigned char) c;
        }
    }
    return i;
}
