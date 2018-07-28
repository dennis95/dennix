/* Copyright (c) 2018 Dennis Wölfing
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

/* sh/builtins.c
 * Shell builtins.
 */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "builtins.h"

static int cd(int argc, char* argv[]);
static int sh_umask(int argc, char* argv[]);

struct builtin builtins[] = {
    { "cd", cd },
    { "umask", sh_umask },
    { NULL, NULL }
};

char* pwd;
static size_t pwdSize;

static void updateLogicalPwd(const char* path) {
    if (pwd && !pwdSize) {
        pwdSize = strlen(pwd);
    }

    if (!pwd) {
        pwd = getcwd(NULL, 0);
        pwdSize = pwd ? strlen(pwd) : 0;
        return;
    }

    if (*path == '/') {
        strcpy(pwd, "/");
    }

    // The resulting string cannot be longer than this.
    size_t newSize = strlen(pwd) + strlen(path) + 2;
    if (newSize > pwdSize) {
        char* newPwd = realloc(pwd, newSize);
        if (!newPwd) {
            free(pwd);
            pwd = NULL;
            return;
        }
        pwd = newPwd;
        pwdSize = newSize;
    }

    char* pwdEnd = pwd + strlen(pwd);
    const char* component = path;
    size_t componentLength = strcspn(component, "/");

    while (*component) {
        if (componentLength == 0 ||
                (componentLength == 1 && strncmp(component, ".", 1) == 0)) {
            // We can ignore this path component.
        } else if (componentLength == 2 && strncmp(component, "..", 2) == 0) {
            char* lastSlash = strrchr(pwd, '/');
            if (lastSlash == pwd) {
                pwdEnd = pwd + 1;
            } else if (lastSlash) {
                pwdEnd = lastSlash;
            }
        } else {
            if (pwdEnd != pwd + 1) {
                *pwdEnd++ = '/';
            }
            memcpy(pwdEnd, component, componentLength);
            pwdEnd += componentLength;
        }

        component += componentLength;
        if (*component) component++;
        componentLength = strcspn(component, "/");
    }

    *pwdEnd = '\0';
}

static int cd(int argc, char* argv[]) {
    const char* newCwd;
    if (argc >= 2) {
        newCwd = argv[1];
    } else {
        newCwd = getenv("HOME");
        if (!newCwd) {
            warnx("HOME not set");
            return 1;
        }
    }

    if (chdir(newCwd) == -1) {
        warn("cd: '%s'", newCwd);
        return 1;
    }

    updateLogicalPwd(newCwd);
    if (!pwd || setenv("PWD", pwd, 1) < 0) {
        unsetenv("PWD");
    }
    return 0;
}

static int sh_umask(int argc, char* argv[]) {
    // TODO: Implement the -S option.
    if (argc > 1) {
        char* end;
        unsigned long value = strtoul(argv[1], &end, 8);
        if (value > 0777 || *end) {
            errno = EINVAL;
            warn("umask: '%s'", argv[1]);
            return 1;
        }
        umask((mode_t) value);
    } else {
        mode_t mask = umask(0);
        umask(mask);
        printf("%.4o\n", (unsigned int) mask);
    }
    return 0;
}
