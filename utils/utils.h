/* Copyright (c) 2017, 2021 Dennis Wölfing
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

/* utils/utils.h
 * Common utility functions.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef DENNIX_VERSION
#  define DENNIX_VERSION ""
#endif

#define UNUSED __attribute__((unused))

static UNUSED bool getConfirmation(void) {
    char* buffer = NULL;
    size_t size = 0;
    if (getline(&buffer, &size, stdin) <= 0) return false;
    bool result = (*buffer == 'y' || *buffer == 'Y');
    free(buffer);
    return result;
}

static UNUSED int help(const char* argv0, const char* helpstr) {
    printf("Usage: %s ", argv0);
    puts(helpstr);
    return 0;
}

static UNUSED int version(const char* argv0) {
    printf("%s (Dennix) %s\n", argv0, DENNIX_VERSION);
    return 0;
}

#endif
