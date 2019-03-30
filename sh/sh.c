/* Copyright (c) 2016, 2017, 2018, 2019 Dennis Wölfing
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

#include <err.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "builtins.h"
#include "execute.h"
#include "expand.h"
#include "parser.h"
#include "tokenizer.h"

#ifndef DENNIX_VERSION
#  define DENNIX_VERSION ""
#endif

bool inputIsTerminal;
struct termios termios;

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    int c;
    while ((c = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            printf("Usage: %s %s\n", argv[0], "[OPTIONS]\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
            return 0;
        case 1:
            printf("%s (Dennix) %s\n", argv[0], DENNIX_VERSION);
            return 0;
        case '?':
            return 1;
        }
    }

    // Ignore signals that should not terminate the (interactive) shell.
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    inputIsTerminal = isatty(0);
    if (inputIsTerminal) {
        tcgetattr(0, &termios);
    }

    pwd = getenv("PWD");
    if (pwd) {
        pwd = strdup(pwd);
    } else {
        pwd = getcwd(NULL, 0);
        if (pwd) {
            setenv("PWD", pwd, 1);
        }
    }

    char* buffer = NULL;
    size_t bufferSize = 0;

    const char* username = getlogin();
    if (!username) {
        username = "?";
    }
    char hostname[HOST_NAME_MAX + 1];
    if (gethostname(hostname, sizeof(hostname)) < 0) {
        strcpy(hostname, "?");
    }

    while (true) {
        fprintf(stderr, "\e[32m%s@%s \e[1;36m%s $\e[22;39m ",
                username, hostname, pwd ? pwd : ".");

        struct Tokenizer tokenizer;
        enum TokenizerResult tokenResult;
        if (!initTokenizer(&tokenizer)) err(1, "initTokenizer");

continue_tokenizing:
        do {
            ssize_t length = getline(&buffer, &bufferSize, stdin);
            if (length < 0) {
                putchar('\n');
                if (feof(stdin)) exit(0);
                err(1, NULL);
            }

            tokenResult = splitTokens(&tokenizer, buffer);
            if (tokenResult == TOKENIZER_ERROR) {
                err(1, "Tokenizer error");
            } else if (tokenResult == TOKENIZER_NEED_INPUT) {
                fputs("> ", stderr);
            }
        } while (tokenResult == TOKENIZER_NEED_INPUT);

        if (tokenResult != TOKENIZER_DONE) {
            freeTokenizer(&tokenizer);
            continue;
        }

        if (tokenizer.numTokens == 1) {
            // Only a newline token was found.
            freeTokenizer(&tokenizer);
            continue;
        }

        struct Parser parser;
        initParser(&parser, &tokenizer);

        struct CompleteCommand command;
        enum ParserResult parserResult = parse(&parser, &command);

        if (parserResult == PARSER_NEWLINE) {
            fputs("> ", stderr);
            goto continue_tokenizing;
        } else if (parserResult == PARSER_ERROR) {
            err(1, "Parser error");
        } else if (parserResult == PARSER_MATCH) {
            execute(&command);
            freeCompleteCommand(&command);
        }

        freeTokenizer(&tokenizer);
    }
}

// Utility functions:

bool addToArray(void** array, size_t* used, void* value, size_t size) {
    void* newArray = reallocarray(*array, size, *used + 1);
    if (!newArray) return false;
    *array = newArray;
    memcpy((void*) ((uintptr_t) *array + size * *used), value, size);
    (*used)++;
    return true;
}

bool moveFd(int old, int new) {
    if (dup2(old, new) < 0) return false;
    if (old != new) {
        close(old);
    }
    return true;
}
