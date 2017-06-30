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

/* libc/src/stdlib/unsetenv.c
 * Unsets the value of an environment variable.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

extern char** environ;
extern char** __mallocedEnviron;
extern size_t __environLength;

int unsetenv(const char* name) {
    if (!*name || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }

    if (!environ) return 0;

    size_t nameLength = strlen(name);

    for (size_t i = 0; environ[i]; i++) {
        if (nameLength == strcspn(environ[i], "=") &&
                strncmp(environ[i], name, nameLength) == 0) {
            if (environ == __mallocedEnviron) {
                // The order of strings does not matter so we can just put the
                // last at the position of the deleted string.
                free(environ[i]);
                environ[i] = environ[--__environLength];
                environ[__environLength] = NULL;
            } else {
                for (size_t j = i; environ[j]; j++) {
                    environ[j] = environ[j + 1];
                }
            }

            i--;
        }
    }

    return 0;
}
