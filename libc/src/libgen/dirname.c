/* Copyright (c) 2016, Dennis Wölfing
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

/* libc/src/libgen/dirname.c
 * Extracts the directory name from a path.
 */

#include <libgen.h>
#include <string.h>

static const char* cwd = ".";

char* dirname(char* path) {
    if (!path || !*path) {
        return (char*) cwd;
    }

    // Remove trailing slashes
    size_t size = strlen(path);
    while (path[size - 1] == '/') {
        path[--size] = '\0';
    }

    if (!*path) {
        *path = '/';
        return path;
    }

    char* lastSlash = strrchr(path, '/');

    if (!lastSlash) {
        return (char*) cwd;
    }

    if (lastSlash == path) {
        // Special case for the root directory which begins with a slash.
        lastSlash++;
    }

    *lastSlash = '\0';
    return path;
}
