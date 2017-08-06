/* Copyright (c) 2017 Dennis Wölfing
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

/* libc/src/stdio/getdelim.c
 * Reads from a file until a given delimiter.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

ssize_t getdelim(char** restrict lineptr, size_t* restrict size, int delimiter,
        FILE* restrict file) {
    if (!lineptr || !size) {
        errno = EINVAL;
        return -1;
    }

    if (!*lineptr || !*size) {
        *size = 80;
        char* newBuffer = realloc(*lineptr, *size);
        if (!newBuffer) {
            *size = 0;
            return -1;
        }
        *lineptr = newBuffer;
    }

    flockfile(file);

    size_t i = 0;
    int c;
    do {
        if (i + 1 >= *size) {
            char* newBuffer = reallocarray(*lineptr, 2, *size);
            if (!newBuffer) {
                funlockfile(file);
                return -1;
            }
            *lineptr = newBuffer;
            *size *= 2;
        }

        c = fgetc_unlocked(file);
        if (c == EOF) {
            (*lineptr)[i] = '\0';
            funlockfile(file);
            return i > 0 ? (ssize_t) i : -1;
        }

        (*lineptr)[i++] = c;
    } while (c != delimiter);

    (*lineptr)[i] = '\0';

    funlockfile(file);
    return i;
}
