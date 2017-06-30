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

/* libc/src/stdlib/setenv.c
 * Sets the value of an environment variable.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

extern char** environ;
extern char** __mallocedEnviron;
extern size_t __environLength;
extern size_t __environSize;

static bool resetEnviron(void) {
    // When we application sets environ, we have to copy the environment to a
    // new buffer so that we can modify it. We can free all strings from the
    // old environment because the application is not allowed to use the old
    // strings when it assigns to environ.
    if (__mallocedEnviron) {
        while (*__mallocedEnviron) {
            free(*__mallocedEnviron++);
        }
        free(__mallocedEnviron);
    }

    size_t length = 0;
    if (environ) {
        char** envp = environ;
        while (*envp++) {
            length++;
        }
    }

    size_t allocationSize = length > 15 ? length + 1 : 16;

    __mallocedEnviron = malloc(allocationSize * sizeof(char*));
    if (!__mallocedEnviron) return false;
    __environLength = length;
    __environSize = allocationSize;
    for (size_t i = 0; i < length; i++) {
        __mallocedEnviron[i] = strdup(environ[i]);
    }
    __mallocedEnviron[length] = NULL;
    environ = __mallocedEnviron;

    return true;
}

int setenv(const char* name, const char* value, int overwrite) {
    if (!*name || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }

    if (!environ || environ != __mallocedEnviron) {
        if (!resetEnviron()) return -1;
    }

    size_t nameLength = strlen(name);

    size_t i = 0;
    for (; environ[i]; i++) {
        if (nameLength == strcspn(environ[i], "=") &&
                strncmp(environ[i], name, nameLength) == 0) {
            if (!overwrite) return 0;

            // Replace the old string by the new one.
            size_t entryLength = nameLength + strlen(value) + 2;
            char* newEntry = malloc(entryLength);
            if (!newEntry) return -1;
            stpcpy(stpcpy(stpcpy(newEntry, name), "="), value);
            free(environ[i]);
            environ[i] = newEntry;
            return 0;
        }
    }

    // We need to add a new string to the environment.
    if (__environLength + 1 == __environSize) {
        char** newEnviron = reallocarray(environ,
                __environSize, 2 * sizeof(char*));
        if (!newEnviron) return -1;
        environ = __mallocedEnviron = newEnviron;
        __environSize *= 2;
    }

    size_t entryLength = nameLength + strlen(value) + 2;
    environ[i] = malloc(entryLength);
    if (!environ[i]) return -1;
    stpcpy(stpcpy(stpcpy(environ[i], name), "="), value);
    environ[i + 1] = NULL;
    __environLength++;
    return 0;
}
