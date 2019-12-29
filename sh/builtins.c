/* Copyright (c) 2018, 2019 Dennis Wölfing
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
#include "variables.h"

static int cd(int argc, char* argv[]);
static int sh_umask(int argc, char* argv[]);

struct builtin builtins[] = {
    { "cd", cd },
    { "umask", sh_umask },
    { NULL, NULL }
};

char* pwd;

static char* getNewLogicalPwd(const char* oldPwd, const char* dir) {
    if (*dir == '/') {
        oldPwd = "/";
    }

    // The resulting string cannot be longer than this.
    size_t newSize = strlen(oldPwd) + strlen(dir) + 2;
    char* newPwd = malloc(newSize);
    if (!newPwd) return NULL;

    char* pwdEnd = stpcpy(newPwd, oldPwd);
    const char* component = dir;
    size_t componentLength = strcspn(component, "/");

    while (*component) {
        if (componentLength == 0 ||
                (componentLength == 1 && strncmp(component, ".", 1) == 0)) {
            // We can ignore this path component.
        } else if (componentLength == 2 && strncmp(component, "..", 2) == 0) {
            char* lastSlash = strrchr(newPwd, '/');
            if (lastSlash == newPwd) {
                pwdEnd = newPwd + 1;
            } else if (lastSlash) {
                pwdEnd = lastSlash;
            }
        } else {
            if (pwdEnd != newPwd + 1) {
                *pwdEnd++ = '/';
            }
            memcpy(pwdEnd, component, componentLength);
            pwdEnd += componentLength;
        }

        component += componentLength;
        if (*component) component++;
        componentLength = strcspn(component, "/");
        *pwdEnd = '\0';
    }

    return newPwd;
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

    if (!pwd) {
        if (chdir(newCwd) < 0) {
            warn("cd: '%s'", newCwd);
            return 1;
        }

        pwd = getcwd(NULL, 0);
    } else {
        char* newPwd = getNewLogicalPwd(pwd, newCwd);
        if (!newPwd || chdir(newPwd) < 0) {
            warn("cd: '%s'", newCwd);
            free(newPwd);
            return 1;
        }

        free(pwd);
        pwd = newPwd;
    }

    if (pwd) {
        setVariable("PWD", pwd, true);
    } else {
        unsetVariable("PWD");
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
