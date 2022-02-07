/* Copyright (c) 2018, 2019, 2020, 2021, 2022 Dennis Wölfing
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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "builtins.h"
#include "sh.h"
#include "variables.h"

static int cd(int argc, char* argv[]);
static int colon(int argc, char* argv[]);
static int sh_exit(int argc, char* argv[]);
static int export(int argc, char* argv[]);
static int sh_umask(int argc, char* argv[]);
static int unset(int argc, char* argv[]);

const struct builtin builtins[] = {
    { ":", colon, BUILTIN_SPECIAL }, // : must be the first entry in this list.
    { "cd", cd, 0 },
    { "exit", sh_exit, BUILTIN_SPECIAL },
    { "export", export, BUILTIN_SPECIAL },
    { "umask", sh_umask, 0 },
    { "unset", unset, BUILTIN_SPECIAL },
    { NULL, NULL, 0 }
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
        newCwd = getVariable("HOME");
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

static int colon(int argc, char* argv[]) {
    (void) argc; (void) argv;
    return 0;
}

static int sh_exit(int argc, char* argv[]) {
    if (argc > 2) {
        warnx("exit: too many arguments");
    }
    if (argc >= 2) {
        char* end;
        long value = strtol(argv[1], &end, 10);
        if (value < INT_MIN || value > INT_MAX || *end) {
            warnx("exit: invalid exit status '%s'", argv[1]);
            lastStatus = 255;
        } else {
            lastStatus = value;
        }
    }

    exit(lastStatus);
}

static int export(int argc, char* argv[]) {
    bool print = false;
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') break;
        if (argv[i][1] == '-' && argv[i][2] == '\0') {
            i++;
            break;
        }
        for (size_t j = 1; argv[i][j]; j++) {
            if (argv[i][j] == 'p') {
                print = true;
            } else {
                warnx("export: invalid option '-%c'", argv[i][j]);
                return 1;
            }
        }
    }

    if (print && i < argc) {
        warnx("export: extra operand '%s'", argv[i]);
        return 1;
    }

    if (print || i == argc) {
        printEnvVariables();
        return 0;
    }

    bool success = true;
    for (; i < argc; i++) {
        char* equals = strchr(argv[i], '=');
        if (equals) *equals = '\0';

        if (!isRegularVariableName(argv[i])) {
            warnx("export: '%s' is not a valid name", argv[i]);
            success = false;
            continue;
        }
        setVariable(argv[i], equals ? equals + 1 : NULL, true);
    }
    return success ? 0 : 1;
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

static int unset(int argc, char* argv[]) {
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') break;
        if (argv[i][1] == '-' && argv[i][2] == '\0') {
            i++;
            break;
        }
        for (size_t j = 1; argv[i][j]; j++) {
            if (argv[i][j] == 'v') {
                // ignored
            } else {
                warnx("unset: invalid option '-%c'", argv[i][j]);
                return 1;
            }
        }
    }

    bool success = true;
    for (; i < argc; i++) {
        if (!isRegularVariableName(argv[i])) {
            warnx("unset: '%s' is not a valid name", argv[i]);
            success = false;
            continue;
        }
        unsetVariable(argv[i]);
    }
    return success ? 0 : 1;
}
