/* Copyright (c) 2017, 2022 Dennis Wölfing
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

/* libc/src/unistd/getcwd.c
 * Gets the current working directory. (POSIX2008)
 */

#define canonicalize_file_name __canonicalize_file_name
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char* getcwd(char* buffer, size_t size) {
    if (buffer && size == 0) {
        errno = EINVAL;
        return NULL;
    }

    char* result = canonicalize_file_name(".");
    if (!result) return NULL;

    if (!buffer) {
        // As an extension we allocate the buffer for the caller.
        return result;
    }

    if (strlcpy(buffer, result, size) >= size) {
        free(result);
        errno = ERANGE;
        return NULL;
    }
    free(result);
    return buffer;
}
