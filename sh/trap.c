/* Copyright (c) 2022 Dennis Wölfing
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

/* sh/trap.c
 * Traps.
 */

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "execute.h"
#include "trap.h"

#ifndef NSIG_MAX
#  define NSIG_MAX (sizeof(sigset_t) * CHAR_BIT + 1)
#endif

enum {
    DEFAULT,
    IGNORED,
    TRAPPED,
    ALWAYS_IGNORED,
    INVALID,
};

bool executingTrap = false;
volatile sig_atomic_t trapsPending;

static sigset_t caughtSignals;
static bool executingExitTrap = false;
static char* traps[NSIG_MAX];
static int trapStates[NSIG_MAX];

static void sigintHandler(int signo) {
    (void) signo;
}

static void signalHandler(int signum) {
    sigaddset(&caughtSignals, signum);
    trapsPending = 1;
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

static void executeTrapAction(int condition) {
    struct Parser parser;
    const char* context = traps[condition];
    initParser(&parser, readInputFromString, &context);

    struct CompleteCommand command;
    enum ParserResult parserResult = parse(&parser, &command, true);

    if (parserResult == PARSER_MATCH) {
        int status = lastStatus;
        executingTrap = true;
        execute(&command);
        executingTrap = false;
        freeCompleteCommand(&command);
        lastStatus = status;
    }

    freeParser(&parser);
}

void blockTraps(const sigset_t* mask) {
    sigprocmask(SIG_SETMASK, mask, NULL);

    if (trapsPending) {
        trapsPending = 0;

        for (int i = 1; i < NSIG_MAX; i++) {
            if (sigismember(&caughtSignals, i)) {
                executeTrapAction(i);
            }
        }
        sigemptyset(&caughtSignals);
    }
}

void executeTraps(void) {
    sigset_t set;
    unblockTraps(&set);
    blockTraps(&set);
}

noreturn void exitShell(int status) {
    if (!executingExitTrap && trapStates[0] == TRAPPED) {
        executingExitTrap = true;
        executeTrapAction(0);
    }

    exit(status);
}

void initializeTraps(void) {
    for (int i = 1; i < NSIG_MAX; i++) {
        struct sigaction sa;
        if (sigaction(i, NULL, &sa) == 0) {
            if (sa.sa_handler == SIG_IGN) {
                trapStates[i] = ALWAYS_IGNORED;
            }
        } else {
            trapStates[i] = INVALID;
        }
    }

    sigemptyset(&caughtSignals);

    if (shellOptions.interactive) {
        // These signals need to be handled specially. We do not change
        // trapStates as the trap builtin does not need to know about this.
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGINT);
        sigprocmask(SIG_BLOCK, &set, NULL);

        signal(SIGINT, sigintHandler);

        signal(SIGQUIT, SIG_IGN);
        signal(SIGTERM, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
    }
}

void resetSignals(void) {
    for (int i = 1; i < NSIG_MAX; i++) {
        if (trapStates[i] == INVALID) continue;

        if (trapStates[i] != IGNORED && trapStates[i] != ALWAYS_IGNORED) {
            struct sigaction sa;
            sa.sa_handler = SIG_DFL;
            sa.sa_flags = 0;
            sigemptyset(&sa.sa_mask);
            sigaction(i, &sa, NULL);
        }
    }

    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);
}

void resetTraps(void) {
    sigset_t unblockSet;
    sigemptyset(&unblockSet);

    for (int i = 0; i < NSIG_MAX; i++) {
        if (trapStates[i] == TRAPPED) {
            free(traps[i]);
            traps[i] = NULL;
            trapStates[i] = DEFAULT;

            if (i != 0) {
                struct sigaction sa;
                sa.sa_handler = SIG_DFL;
                sa.sa_flags = 0;
                sigemptyset(&sa.sa_mask);
                sigaction(i, &sa, NULL);

                sigaddset(&unblockSet, i);
            }
        }
    }

    sigprocmask(SIG_UNBLOCK, &unblockSet, NULL);
}

static int parseCondition(const char* name) {
    if (strcmp(name, "EXIT") == 0 || strcmp(name, "0") == 0) {
        return 0;
    } else {
        int condition;
        if (str2sig(name, &condition) != 0) {
            return -1;
        }
        return condition;
    }
}

int trap(int argc, char* argv[]) {
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
                warnx("trap: invalid option '-%c'", argv[i][j]);
                return 1;
            }
        }
    }

    int status = 0;

    // TODO: trap -p in command substitutions is supposed to print the old traps
    // even though they have been reset.
    if (i == argc) {
        for (int i = 0; i < NSIG_MAX; i++) {
            if (trapStates[i] == INVALID) continue;

            if (trapStates[i] != DEFAULT || print) {
                const char* action = traps[i] ? traps[i] : "-";
                char buffer[SIG2STR_MAX];
                if (i == 0) {
                    strcpy(buffer, "EXIT");
                } else {
                    sig2str(i, buffer);
                }

                fputs("trap -- ", stdout);
                printQuoted(action);
                printf(" %s\n", buffer);
            }
        }
        return 0;
    } else if (print) {
        for (; i < argc; i++) {
            int condition = parseCondition(argv[i]);
            if (condition < 0) {
                warnx("trap: invalid condition '%s'", argv[i]);
                status = 1;
                continue;
            }

            const char* action = traps[condition] ? traps[condition] : "-";
            char buffer[SIG2STR_MAX];
            if (condition == 0) {
                strcpy(buffer, "EXIT");
            } else {
                sig2str(condition, buffer);
            }

            fputs("trap -- ", stdout);
            printQuoted(action);
            printf(" %s\n", buffer);
        }
        return status;
    }

    const char* action = NULL;
    char* end;
    errno = 0;
    strtol(argv[i], &end, 10);
    if (errno || *end) {
        action = argv[i];
        i++;
    }

    for (; i < argc; i++) {
        int condition = parseCondition(argv[i]);
        if (condition < 0) {
            warnx("trap: invalid condition '%s'", argv[i]);
            status = 1;
            continue;
        }

        if (!action || strcmp(action, "-") == 0) {
            free(traps[condition]);
            traps[condition] = NULL;
            if (trapStates[condition] != ALWAYS_IGNORED) {
                trapStates[condition] = DEFAULT;
            }
        } else if (*action == '\0') {
            free(traps[condition]);
            traps[condition] = strdup(action);
            if (!traps[condition]) err(1, "malloc");
            if (trapStates[condition] != ALWAYS_IGNORED) {
                trapStates[condition] = IGNORED;
            }
        } else {
            free(traps[condition]);
            traps[condition] = strdup(action);
            if (!traps[condition]) err(1, "malloc");
            if (trapStates[condition] != ALWAYS_IGNORED) {
                trapStates[condition] = TRAPPED;
            }
        }

        if (condition != 0) {
            sigset_t set;
            sigemptyset(&set);
            sigaddset(&set, condition);
            sigprocmask(SIG_BLOCK, &set, NULL);

            void (*handler)(int);
            if (trapStates[condition] == IGNORED ||
                    trapStates[condition] == ALWAYS_IGNORED) {
                handler = SIG_IGN;
            } else if (trapStates[condition] == TRAPPED) {
                handler = signalHandler;
            } else if (shellOptions.interactive && condition == SIGINT) {
                handler = sigintHandler;
            } else if (shellOptions.interactive && (condition == SIGQUIT ||
                    condition == SIGTERM || condition == SIGTSTP ||
                    condition == SIGTTIN || condition == SIGTTOU)) {
                handler = SIG_IGN;
            } else {
                handler = SIG_DFL;
            }

            struct sigaction sa;
            sa.sa_handler = handler;
            sa.sa_flags = 0;
            sigemptyset(&sa.sa_mask);
            sigaction(condition, &sa, NULL);

            if (handler == SIG_DFL || handler == SIG_IGN) {
                sigprocmask(SIG_UNBLOCK, &set, NULL);
            }
        }
    }

    return status;
}

void unblockTraps(sigset_t* mask) {
    sigset_t empty;
    sigemptyset(&empty);

    sigprocmask(SIG_SETMASK, &empty, mask);
}
