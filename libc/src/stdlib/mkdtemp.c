/* Copyright (c) 2018, 2021 Dennis Wölfing
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

/* libc/src/stdlib/mkdtemp.c
 * Create a temporary directory.
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char* letters =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

char* mkdtemp(char* template) {
    size_t length = strlen(template);
    if (length < 6) {
        errno = EINVAL;
        return NULL;
    }

    char* xtemplate = template + length - 6;
    if (strcmp(xtemplate, "XXXXXX") != 0) {
        errno = EINVAL;
        return NULL;
    }

    do {
        uint64_t value;
        arc4random_buf(&value, sizeof(value));
        for (size_t i = 0; i < 6; i++) {
            xtemplate[i] = letters[value % 64];
            value /= 64;
        }

        if (mkdir(template, 0700) == 0) return template;
    } while (errno == EEXIST);

    return NULL;
}
