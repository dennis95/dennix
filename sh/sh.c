/* Copyright (c) 2016, 2017, 2018 Dennis Wölfing
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "builtins.h"
#include "expand.h"
#include "tokenizer.h"

#ifndef DENNIX_VERSION
#  define DENNIX_VERSION ""
#endif

static int executeCommand(int argc, char* arguments[]);
static const char* getExecutablePath(const char* command);

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

        // TODO: Parse into simple commands and compound commands
        // For now just remove the newline operator at the end.

        size_t numTokens = tokenizer.numTokens - 1;
        char** tokens = malloc((numTokens + 1) * sizeof(char*));
        if (!tokens) err(1, "malloc");
        tokens[numTokens] = NULL;

        // Word expansion
        for (size_t i = 0; i < numTokens; i++) {
            tokens[i] = expandWord(tokenizer.tokens[i]);
            if (!tokens[i]) err(1, "failed to expand");
        }
        freeTokenizer(&tokenizer);

        if (numTokens > 0) {
            executeCommand(numTokens, tokens);
        }

        for (size_t i = 0; i < numTokens; i++) {
            free(tokens[i]);
        }
        free(tokens);
    }
}

static int executeCommand(int argc, char* arguments[]) {
    const char* command = arguments[0];
    // Special built-ins
    if (strcmp(command, "exit") == 0) {
        exit(0);
    }

    // Regular built-ins
    if (strcmp(command, "cd") == 0) {
        return cd(argc, arguments);
    }

    pid_t pid = fork();

    if (pid < 0) {
        warn("fork");
        return -1;
    } else if (pid == 0) {
        if (!strchr(command, '/')) {
            command = getExecutablePath(command);
        }

        if (command) {
            execv(command, arguments);
            warn("execv: '%s'", command);
        } else {
            warnx("'%s': Command not found", arguments[0]);
        }
        _Exit(127);
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            err(1, "waitpid");
        }

        if (WIFSIGNALED(status)) {
            int signum = WTERMSIG(status);
            if (signum == SIGINT) {
                fputc('\n', stderr);
            } else {
                fprintf(stderr, "%s\n", strsignal(signum));
            }
            return 128 + signum;
        }

        return WEXITSTATUS(status);
    }
}

static const char* getExecutablePath(const char* command) {
    size_t commandLength = strlen(command);
    const char* path = getenv("PATH");

    while (*path) {
        size_t length = strcspn(path, ":");
        char* buffer = malloc(commandLength + length + 2);
        if (!buffer) err(1, "malloc");

        memcpy(buffer, path, length);
        buffer[length] = '/';
        memcpy(buffer + length + 1, command, commandLength);
        buffer[commandLength + length + 1] = '\0';

        if (access(buffer, X_OK) == 0) {
            return buffer;
        }

        free(buffer);
        path += length + 1;
    }

    return NULL;
}
