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
#include "execute.h"
#include "stringbuffer.h"
#include "trap.h"
#include "variables.h"

static int sh_break(int argc, char* argv[]);
static int cd(int argc, char* argv[]);
static int colon(int argc, char* argv[]);
static int sh_continue(int argc, char* argv[]);
static int dot(int argc, char* argv[]);
static int eval(int argc, char* argv[]);
static int exec(int argc, char* argv[]);
static int sh_exit(int argc, char* argv[]);
static int export(int argc, char* argv[]);
static int sh_return(int argc, char* argv[]);
static int set(int argc, char* argv[]);
static int shift(int argc, char* argv[]);
static int sh_umask(int argc, char* argv[]);
static int unset(int argc, char* argv[]);

const struct builtin builtins[] = {
    { ":", colon, BUILTIN_SPECIAL }, // : must be the first entry in this list.
    { "break", sh_break, BUILTIN_SPECIAL },
    { "cd", cd, 0 },
    { "continue", sh_continue, BUILTIN_SPECIAL },
    { ".", dot, BUILTIN_SPECIAL },
    { "eval", eval, BUILTIN_SPECIAL },
    { "exec", exec, BUILTIN_SPECIAL },
    { "exit", sh_exit, BUILTIN_SPECIAL },
    { "export", export, BUILTIN_SPECIAL },
    { "return", sh_return, BUILTIN_SPECIAL },
    { "set", set, BUILTIN_SPECIAL },
    { "shift", shift, BUILTIN_SPECIAL },
    { "trap", trap, BUILTIN_SPECIAL },
    { "umask", sh_umask, 0 },
    { "unset", unset, BUILTIN_SPECIAL },
    { NULL, NULL, 0 }
};

static int sh_break(int argc, char* argv[]) {
    if (argc > 2) {
        warnx("break: too many arguments");
        return 1;
    }
    unsigned long breaks = 1;
    if (argc == 2) {
        char* end;
        errno = 0;
        breaks = strtoul(argv[1], &end, 10);
        if (errno || breaks == 0 || *end) {
            warnx("break: invalid number '%s'", argv[1]);
            return 1;
        }
    }

    if (!loopCounter) {
        warnx("break: used outside of loop");
        return 1;
    }

    numBreaks = breaks;
    if (numBreaks > loopCounter) {
        numBreaks = loopCounter;
    }
    return 0;
}

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

static int sh_continue(int argc, char* argv[]) {
    if (argc > 2) {
        warnx("continue: too many arguments");
        return 1;
    }
    unsigned long continues = 1;
    if (argc == 2) {
        char* end;
        errno = 0;
        continues = strtoul(argv[1], &end, 10);
        if (errno || continues == 0 || *end) {
            warnx("continue: invalid number '%s'", argv[1]);
            return 1;
        }
    }

    if (!loopCounter) {
        warnx("continue: used outside of loop");
        return 1;
    }

    numContinues = continues;
    if (numContinues > loopCounter) {
        numContinues = loopCounter;
    }
    return 0;
}

struct DotContext {
    FILE* file;
    char* buffer;
    size_t bufferSize;
};

static bool readInputFromFile(const char** str, bool newCommand,
        void* context) {
    (void) newCommand;
    struct DotContext* ctx = context;
    FILE* file = ctx->file;

    ssize_t length = getline(&ctx->buffer, &ctx->bufferSize, file);

    if (length < 0 && !feof(file)) err(1, "getline");
    if (length < 0) return false;

    *str = ctx->buffer;
    return true;
}

static int dot(int argc, char* argv[]) {
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') break;
        if (argv[i][1] == '-' && argv[i][2] == '\0') {
            i++;
            break;
        }
        warnx(".: invalid option '-%c'", argv[i][1]);
        return 1;
    }

    if (i >= argc) {
        warnx(".: missing file operand");
        return 1;
    }
    if (i + 1 < argc) {
        warnx(".: too many arguments");
        return 1;
    }

    char* pathname = argv[i];
    if (!strchr(pathname, '/')) {
        pathname = getExecutablePath(pathname, false);
        if (!pathname) {
            errno = ENOENT;
            warn(".: '%s'", argv[i]);
            return 1;
        }
    }

    FILE* file = fopen(pathname, "r");
    if (!file) {
        warn(".: '%s'", pathname);
        if (pathname != argv[i]) {
            free(pathname);
        }
        return 1;
    }
    if (pathname != argv[i]) {
        free(pathname);
    }

    struct DotContext context;
    context.file = file;
    context.buffer = NULL;
    context.bufferSize = 0;

    struct Parser parser;
    initParser(&parser, readInputFromFile, &context);
    struct CompleteCommand command;
    enum ParserResult parserResult = parse(&parser, &command, true);
    freeParser(&parser);
    free(context.buffer);
    fclose(file);

    int status = 1;
    if (parserResult == PARSER_MATCH) {
        status = execute(&command);
        freeCompleteCommand(&command);
    } else if (parserResult == PARSER_NO_CMD) {
        status = 0;
    }
    return status;
}

static bool readInputFromString(const char** str, bool newCommand,
        void* context) {
    (void) newCommand;

    const char** word = context;
    if (!*word) return false;
    *str = *word;
    *word = NULL;
    return true;
}

static int eval(int argc, char* argv[]) {
    struct StringBuffer buffer;
    initStringBuffer(&buffer);

    for (int i = 1; i < argc; i++) {
        if (i != 1) {
            appendToStringBuffer(&buffer, ' ');
        }
        appendStringToStringBuffer(&buffer, argv[i]);
    }
    appendToStringBuffer(&buffer, '\n');

    char* string = finishStringBuffer(&buffer);
    struct Parser parser;
    const char* context = string;
    initParser(&parser, readInputFromString, &context);

    struct CompleteCommand command;
    enum ParserResult parserResult = parse(&parser, &command, true);

    int status = 0;
    if (parserResult == PARSER_MATCH) {
        status = execute(&command);
        freeCompleteCommand(&command);
    } else if (parserResult == PARSER_SYNTAX) {
        status = 1;
    }

    freeParser(&parser);
    free(string);
    return status;
}

static int exec(int argc, char* argv[]) {
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') break;
        if (argv[i][1] == '-' && argv[i][2] == '\0') {
            i++;
            break;
        }
        warnx("exec: invalid option '-%c'", argv[i][1]);
        return 1;
    }

    if (i == argc) return 0;
    executeUtility(argc - i, argv + i, NULL, 0);
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

    exitShell(lastStatus);
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
        printVariables(true);
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

static int sh_return(int argc, char* argv[]) {
    if (argc > 2) {
        warnx("return: too many arguments");
        returning = true;
        returnStatus = 1;
        return 1;
    }

    int status = lastStatus;

    if (argc == 2) {
        char* end;
        errno = 0;
        long value = strtol(argv[1], &end, 10);
        if (errno || value < INT_MIN || value > INT_MAX || *end) {
            warnx("return: invalid number '%s'", argv[1]);
            status = 1;
        } else {
            status = value;
        }
    }

    returning = true;
    returnStatus = status;
    return status;
}

static void printOptionStatus(bool plusOption, const char* optionName,
        bool optionValue) {
    if (plusOption) {
        printf("set %co %s\n", optionValue ? '-' : '+', optionName);
    } else {
        printf("%-16s%s\n", optionName, optionValue ? "on" : "off");
    }
}

static void printOptions(bool plusOption) {
    printOptionStatus(plusOption, "allexport", shellOptions.allexport);
    printOptionStatus(plusOption, "errexit", shellOptions.errexit);
    printOptionStatus(plusOption, "hashall", shellOptions.hashall);
    printOptionStatus(plusOption, "ignoreeof", shellOptions.ignoreeof);
    printOptionStatus(plusOption, "monitor", shellOptions.monitor);
    printOptionStatus(plusOption, "noclobber", shellOptions.noclobber);
    printOptionStatus(plusOption, "noexec", shellOptions.noexec);
    printOptionStatus(plusOption, "noglob", shellOptions.noglob);
    printOptionStatus(plusOption, "nolog", shellOptions.nolog);
    printOptionStatus(plusOption, "notify", shellOptions.notify);
    printOptionStatus(plusOption, "nounset", shellOptions.nounset);
    printOptionStatus(plusOption, "verbose", shellOptions.verbose);
    printOptionStatus(plusOption, "vi", shellOptions.vi);
    printOptionStatus(plusOption, "xtrace", shellOptions.xtrace);
}

static int set(int argc, char* argv[]) {
    bool setArguments = false;
    int i;
    for (i = 1; i < argc; i++) {
        if ((argv[i][0] != '-' && argv[i][0] != '+') || argv[i][1] == '\0') {
            break;
        }
        if (argv[i][0] == '-' && argv[i][1] == '-' && argv[i][2] == '\0') {
            i++;
            setArguments = true;
            break;
        }

        const char* arg = argv[i];
        bool plusOption = arg[0] == '+';
        for (size_t j = 1; arg[j]; j++) {
            if (!handleShortOption(plusOption, arg[j])) {
                if (arg[j] != 'o') {
                    warnx("set: invalid option '%c%c'", arg[0], arg[j]);
                    return 1;
                }

                if (arg[j + 1]) {
                    warnx("set: unexpected '%c' after %co", arg[j + 1],
                            arg[0]);
                    return 1;
                }

                const char* option = argv[++i];
                if (!option) {
                    printOptions(plusOption);
                    return 0;
                }

                if (!handleLongOption(plusOption, option)) {
                    warnx("set: invalid option name '%s'", option);
                    return 1;
                }
                break;
            }
        }
    }

    if (argc == 1) {
        printVariables(false);
        return 0;
    }

    if (i < argc || setArguments) {
        int numArgs = argc - i;
        char** newArguments = malloc((numArgs + 1) * sizeof(char*));
        if (!newArguments) err(1, "malloc");
        newArguments[0] = arguments[0];
        for (int j = 0; j < numArgs; j++) {
            newArguments[j + 1] = strdup(argv[i + j]);
            if (!newArguments[j + 1]) err(1, "malloc");
        }

        for (int j = 1; j <= numArguments; j++) {
            free(arguments[j]);
        }
        free(arguments);
        arguments = newArguments;
        numArguments = numArgs;
    }

    return 0;
}

static int shift(int argc, char* argv[]) {
    if (argc > 2) {
        warnx("shift: too many arguments");
        return 1;
    }

    long n = 1;
    if (argc == 2) {
        char* end;
        errno = 0;
        n = strtol(argv[1], &end, 10);
        if (errno || n < 0 || *end) {
            warnx("shift: invalid number '%s'", argv[1]);
            return 1;
        }
    }

    if (n == 0) return 0;

    int newNumArguments = numArguments < n ? 0 : numArguments - n;
    char** newArguments = malloc((newNumArguments + 1) * sizeof(char*));
    if (!newArguments) err(1, "malloc");
    newArguments[0] = arguments[0];
    for (int i = 1; i <= newNumArguments; i++) {
        newArguments[i] = arguments[i + n];
    }

    for (int i = 1; i <= n && i <= numArguments; i++) {
        free(arguments[i]);
    }
    free(arguments);
    arguments = newArguments;
    numArguments = newNumArguments;

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

static int unset(int argc, char* argv[]) {
    bool function = false;
    bool variable = false;

    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') break;
        if (argv[i][1] == '-' && argv[i][2] == '\0') {
            i++;
            break;
        }
        for (size_t j = 1; argv[i][j]; j++) {
            if (argv[i][j] == 'f') {
                function = true;
            } else if (argv[i][j] == 'v') {
                variable = true;
            } else {
                warnx("unset: invalid option '-%c'", argv[i][j]);
                return 1;
            }
        }
    }

    if (!function && !variable) {
        variable = true;
    }

    bool success = true;
    for (; i < argc; i++) {
        if (!isRegularVariableName(argv[i])) {
            warnx("unset: '%s' is not a valid name", argv[i]);
            success = false;
            continue;
        }
        if (variable) {
            unsetVariable(argv[i]);
        }
        if (function) {
            unsetFunction(argv[i]);
        }
    }
    return success ? 0 : 1;
}
