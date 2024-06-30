/* Copyright (c) 2017, 2022, 2024 Dennis Wölfing
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

/* libc/src/stdlib/reallocarray.c
 * Changes the size of an allocation. (POSIX2024, called from C89)
 */

#include <errno.h>
#include <stdlib.h>

void* __reallocarray(void* ptr, size_t size1, size_t size2) {
    size_t resultSize;
    if (__builtin_mul_overflow(size1, size2, &resultSize)) {
        errno = ENOMEM;
        return NULL;
    }
    return realloc(ptr, resultSize);
}
__weak_alias(__reallocarray, reallocarray);
