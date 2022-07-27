/* Copyright (c) 2016, 2017, 2018, 2019, 2020, 2021, 2022 Dennis Wölfing
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

/* sh/sh.c
 * The shell.
 */

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "execute.h"
#include "expand.h"
#include "interactive.h"
#include "parser.h"
#include "variables.h"

#ifndef DENNIX_VERSION
#  define DENNIX_VERSION ""
#endif

struct CompleteCommand* currentCommand;
bool endOfFileReached;
bool inputIsTerminal;
int lastStatus;
struct ShellOptions shellOptions;

static char* buffer;
static size_t bufferSize;
static char hostname[HOST_NAME_MAX + 1];
static FILE* inputFile;
static bool interactiveInput;
static jmp_buf jumpBuffer;
static const char* username;

static void help(const char* argv0);
static int parseOptions(int argc, char* argv[]);
static void readCommand(const char** str, bool newCommand, void* context);

int main(int argc, char* argv[]) {
    int optionIndex = parseOptions(argc, argv);
    numArguments = argc - optionIndex;

    if (shellOptions.command) {
        if (numArguments == 0) errx(1, "The -c option requires an operand.");
        optionIndex++;
        numArguments--;
    }

    initializeVariables();

    if (numArguments == 0) {
        arguments = malloc(sizeof(char*));
        if (!arguments) err(1, "malloc");
        arguments[0] = strdup(argv[0]);
        if (!arguments[0]) err(1, "strdup");
    } else {
        numArguments--;
        if (shellOptions.stdInput) {
            numArguments++;
            optionIndex--;
        }
        arguments = malloc((numArguments + 1) * sizeof(char*));
        if (!arguments) err(1, "malloc");
        if (shellOptions.stdInput) {
            arguments[0] = strdup(argv[0]);
            if (!arguments[0]) err(1, "strdup");
        }

        for (int i = shellOptions.stdInput; i <= numArguments; i++) {
            arguments[i] = strdup(argv[optionIndex + i]);
            if (!arguments[i]) err(1, "strdup");
        }
    }

    pwd = getenv("PWD");
    if (pwd) {
        pwd = strdup(pwd);
    } else {
        pwd = getcwd(NULL, 0);
        if (pwd) {
            setVariable("PWD", pwd, true);
        }
    }

    inputFile = stdin;

    if (shellOptions.command) {
        char* command = argv[optionIndex - 1];
        inputFile = fmemopen(command, strlen(command), "r");
        if (!inputFile) err(1, "fmemopen");
    }

    if (setjmp(jumpBuffer)) {
        shellOptions = (struct ShellOptions) {false};
        assert(arguments[0]);
    }

    if (!shellOptions.command && !shellOptions.stdInput && arguments[0]) {
        int fd = open(arguments[0], O_RDONLY);
        if (fd < 0) err(1, "open: '%s'", arguments[0]);
        // Make sure to use a file descriptor > 10 because the first 10 file
        // descriptors are controlled by the script.
        int fd2 = fcntl(fd, F_DUPFD_CLOEXEC, 10);
        if (fd2 < 0) err(1, "fcntl");
        close(fd);
        inputFile = fdopen(fd2, "r");
        if (!inputFile) err(1, "fdopen: '%s'", arguments[0]);
    }

    // Ignore signals that should not terminate the (interactive) shell.
    if (shellOptions.interactive) {
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTERM, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
    }

    inputIsTerminal = isatty(0);
    interactiveInput = false;
    if (shellOptions.interactive && inputIsTerminal) {
        if (inputFile == stdin) {
            interactiveInput = true;
            initializeInteractive();
        }
    }

    username = getlogin();
    if (!username) {
        username = "?";
    }
    if (gethostname(hostname, sizeof(hostname)) < 0) {
        strcpy(hostname, "?");
    }

    while (true) {
        if (endOfFileReached || feof(inputFile)) {
            if (shellOptions.interactive) {
                fputc('\n', stderr);
            }
            exit(lastStatus);
        }

        struct Parser parser;
        initParser(&parser, readCommand, NULL);
        struct CompleteCommand command;
        enum ParserResult parserResult = parse(&parser, &command, false);
        freeParser(&parser);

        if (parserResult == PARSER_MATCH) {
            fflush(inputFile);

            execute(&command);
            freeCompleteCommand(&command);
        } else if (parserResult == PARSER_SYNTAX) {
            lastStatus = 1;
        }
    }
}

noreturn void executeScript(int argc, char** argv) {
    // Reset all global state and jump back at the beginning of the shell to
    // execute the script.
    freeCompleteCommand(currentCommand);
    freeInteractive();
    freeRedirections();
    unsetFunctions();

    for (int i = 0; i <= numArguments; i++) {
        free(arguments[i]);
    }
    free(arguments);
    numArguments = argc - 1;
    arguments = argv;

    // Unset all nonexported variables.
    initializeVariables();

    if (inputFile != stdin) {
        fclose(inputFile);
    }
    longjmp(jumpBuffer, 1);
}

bool handleShortOption(bool plusOption, char option) {
    switch (option) {
    case 'a': shellOptions.allexport = !plusOption; break;
    case 'b': shellOptions.notify = !plusOption; break;
    case 'C': shellOptions.noclobber = !plusOption; break;
    case 'e': shellOptions.errexit = !plusOption; break;
    case 'f': shellOptions.noglob = !plusOption; break;
    case 'h': shellOptions.hashall = !plusOption; break;
    case 'm': shellOptions.monitor = !plusOption; break;
    case 'n': shellOptions.noexec = !plusOption; break;
    case 'u': shellOptions.nounset = !plusOption; break;
    case 'v': shellOptions.verbose = !plusOption; break;
    case 'x': shellOptions.xtrace = !plusOption; break;

    default: return false;
    }
    return true;
}

bool handleLongOption(bool plusOption, const char* option) {
    if (strcmp(option, "allexport") == 0) {
        shellOptions.allexport = !plusOption;
    } else if (strcmp(option, "errexit") == 0) {
        shellOptions.errexit = !plusOption;
    } else if (strcmp(option, "hashall") == 0) {
        shellOptions.hashall = !plusOption;
    } else if (strcmp(option, "ignoreeof") == 0) {
        shellOptions.ignoreeof = !plusOption;
    } else if (strcmp(option, "monitor") == 0) {
        shellOptions.monitor = !plusOption;
    } else if (strcmp(option, "noclobber") == 0) {
        shellOptions.noclobber = !plusOption;
    } else if (strcmp(option, "noexec") == 0) {
        shellOptions.noexec = !plusOption;
    } else if (strcmp(option, "noglob") == 0) {
        shellOptions.noglob = !plusOption;
    } else if (strcmp(option, "nolog") == 0) {
        shellOptions.nolog = !plusOption;
    } else if (strcmp(option, "notify") == 0) {
        shellOptions.notify = !plusOption;
    } else if (strcmp(option, "nounset") == 0) {
        shellOptions.nounset = !plusOption;
    } else if (strcmp(option, "verbose") == 0) {
        shellOptions.verbose = !plusOption;
    } else if (strcmp(option, "vi") == 0) {
        shellOptions.vi = !plusOption;
    } else if (strcmp(option, "xtrace") == 0) {
        shellOptions.xtrace = !plusOption;
    } else {
        return false;
    }
    return true;
}

static void help(const char* argv0) {
    printf("Usage: %s [OPTIONS] [COMMAND] [ARGUMENT...]\n"
            "  -c                       execute COMMAND\n"
            "  -i                       make shell interactive\n"
            "  -m, -o monitor           enable job control\n"
            "  -o OPTION                enable OPTION\n"
            "  -s                       read from stdin\n"
            "      --help               display this help\n"
            "      --version            display version info\n",
            argv0);
}

static int parseOptions(int argc, char* argv[]) {
    int i;
    for (i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (arg[0] != '-' && arg[0] != '+') break;

        bool plusOption = arg[0] == '+';
        if (!plusOption && (arg[1] == '\0' ||
                (arg[1] == '-' && arg[2] == '\0'))) {
            i++;
            break;
        }

        if (!plusOption && arg[1] == '-') {
            arg += 2;
            if (strcmp(arg, "help") == 0) {
                help(argv[0]);
                exit(0);
            } else if (strcmp(arg, "version") == 0) {
                printf("%s (Dennix) %s\n", argv[0], DENNIX_VERSION);
                exit(0);
            } else {
                errx(1, "unrecognized option '--%s'", arg);
            }
        } else {
            for (size_t j = 1; arg[j]; j++) {
                if (!handleShortOption(plusOption, arg[j])) {
                    if (arg[j] == 'o') {
                        if (arg[j + 1]) {
                            errx(1, "unexpected '%c' after %co", arg[j + 1],
                                    arg[0]);
                        }

                        const char* option = argv[++i];
                        if (!option) {
                            errx(1, "%co requires an argument", arg[0]);
                        }

                        if (!handleLongOption(plusOption, option)) {
                            errx(1, "invalid option name '%s'", option);
                        }
                        break;
                    } else if (!plusOption && arg[j] == 'c') {
                        shellOptions.command = true;
                    } else if (!plusOption && arg[j] == 'i') {
                        shellOptions.interactive = true;
                        shellOptions.monitor = true;
                    } else if (!plusOption && arg[j] == 's') {
                        shellOptions.stdInput = true;
                    } else {
                        errx(1, "invalid option '%c%c'", arg[0], arg[j]);
                    }
                }
            }
        }
    }

    if (shellOptions.command && shellOptions.stdInput) {
        errx(1, "The -c and -s options are mutually exclusive.");
    }

    if (!shellOptions.command && i >= argc) {
        shellOptions.stdInput = true;
    }

    if (shellOptions.stdInput && isatty(0) && isatty(2)) {
        shellOptions.interactive = true;
        shellOptions.monitor = true;
    }

    return i;
}

int printPrompt(bool newCommand) {
    if (newCommand) {
        int length = fprintf(stderr, "\e[32m%s@%s \e[1;36m%s $\e[22;39m ",
                username, hostname, pwd ? pwd : ".");
        // 20 bytes are used for escape sequences.
        return length - 20;
    } else {
        fputs("> ", stderr);
        return 2;
    }
}

static void readCommand(const char** str, bool newCommand, void* context) {
    (void) context;

    if (interactiveInput) {
        return readCommandInteractive(str, newCommand);
    }
    if (shellOptions.interactive && !feof(inputFile)) {
        printPrompt(newCommand);
    }

    ssize_t length = getline(&buffer, &bufferSize, inputFile);

    if (length < 0 && !feof(inputFile)) err(1, "getline");
    if (length > 0 && buffer[length - 1] != '\n') {
        // Make sure that the input ends with a newline.
        if (bufferSize < (size_t) length + 2) {
            buffer = realloc(buffer, length + 2);
            if (!buffer) err(1, "realloc");
        }

        buffer[length] = '\n';
        buffer[length + 1] = '\0';
    }

    *str = buffer;
    if (length < 0) *str = "";
}

// Utility functions:

void addToArray(void** array, size_t* used, const void* value, size_t size) {
    addMultipleToArray(array, used, value, size, 1);
}

void addMultipleToArray(void** array, size_t* used, const void* values,
        size_t size, size_t amount) {
    void* newArray = reallocarray(*array, size, *used + amount);
    if (!newArray) err(1, "realloc");
    *array = newArray;
    memcpy((void*) ((uintptr_t) *array + size * *used), values, size * amount);
    *used += amount;
}

bool moveFd(int old, int new) {
    if (dup2(old, new) < 0) return false;
    if (old != new) {
        close(old);
    }
    return true;
}
