/* Copyright (c) 2019 Dennis Wölfing
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

/* libc/src/unistd/execvp.c
 * Execute a program or script.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int execvp(const char* file, char* const argv[]) {
    if (!*file) {
        errno = ENOENT;
        return -1;
    }

    char* allocatedString = NULL;
    const char* pathname = NULL;
    if (strchr(file, '/')) {
        pathname = file;
    } else {
        const char* path = getenv("PATH");
        while (path) {
            size_t length = strcspn(path, ":");
            if (length == 0) {
                if (access(file, X_OK) == 0) {
                    pathname = file;
                    break;
                }
            } else {
                allocatedString = malloc(length + strlen(file) + 2);
                if (!allocatedString) return -1;
                memcpy(allocatedString, path, length);
                stpcpy(stpcpy(allocatedString + length, "/"), file);
                if (access(allocatedString, X_OK) == 0) {
                    pathname = allocatedString;
                    break;
                }
                free(allocatedString);
                allocatedString = NULL;
            }

            path = path[length] ? path + length + 1 : NULL;
        }

        if (!pathname) {
            errno = ENOENT;
            return -1;
        }
    }

    execv(pathname, argv);

    if (errno != ENOEXEC) {
        free(allocatedString);
        return -1;
    }

    int argc;
    for (argc = 0; argv[argc]; argc++);
    if (argc == 0) argc = 1;
    char** newArgv = malloc((argc + 3) * sizeof(char*));
    if (!newArgv) {
        free(allocatedString);
        return -1;
    }

    newArgv[0] = argv[0] ? argv[0] : (char*) file;
    newArgv[1] = "--";
    newArgv[2] = (char*) pathname;
    for (int i = 1; i < argc; i++) {
        newArgv[i + 2] = argv[i];
    }
    newArgv[argc + 2] = NULL;

    execv("/bin/sh", newArgv);
    free(allocatedString);
    free(newArgv);
    return -1;
}
