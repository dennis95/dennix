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

/* libc/src/dirent/scandir.c
 * Scanning a directory. (POSIX2008)
 */

#define reallocarray __reallocarray
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

int scandir(const char* path, struct dirent*** results,
        int (*selector)(const struct dirent*),
        int (*compare)(const struct dirent**, const struct dirent**)) {
    *results = NULL;
    size_t numEntries = 0;

    DIR* dir = opendir(path);
    if (!dir) return -1;

    while (true) {
        errno = 0;
        struct dirent* entry = readdir(dir);

        if (!entry) {
            if (errno) goto fail;
            break;
        }

        if (selector && !selector(entry)) continue;

        if (numEntries >= (size_t) INT_MAX) {
            errno = EOVERFLOW;
            goto fail;
        }

        struct dirent* entryCopy = malloc(entry->d_reclen);
        if (!entryCopy) goto fail;
        memcpy(entryCopy, entry, entry->d_reclen);

        struct dirent** newList = reallocarray(*results, numEntries + 1,
                sizeof(struct dirent*));
        if (!newList) {
            free(entryCopy);
            goto fail;
        }
        *results = newList;

        newList[numEntries++] = entryCopy;
    }

    qsort(*results, numEntries, sizeof(struct dirent*),
            (int (*)(const void*, const void*)) compare);

    return (int) numEntries;

fail:
    for (size_t i = 0; i < numEntries; i++) {
        free((*results)[i]);
    }
    free(*results);
    closedir(dir);
    return -1;
}
