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

/* sh/execute.c
 * Shell command execution.
 */

#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "builtins.h"
#include "execute.h"
#include "expand.h"

static const char* getExecutablePath(const char* command);
static bool performRedirections(struct Redirection* redirections,
        size_t numRedirections);

int execute(struct SimpleCommand* simpleCommand) {
    char** arguments = malloc((simpleCommand->numWords + 1) * sizeof(char*));
    if (!arguments) err(1, "malloc");
    for (size_t i = 0; i < simpleCommand->numWords; i++) {
        arguments[i] = expandWord(simpleCommand->words[i]);
    }

    int argc = simpleCommand->numWords;
    arguments[argc] = NULL;

    struct Redirection* redirections = malloc(simpleCommand->numRedirections *
            sizeof(struct Redirection));
    if (!redirections) err(1, "malloc");
    for (size_t i = 0; i < simpleCommand->numRedirections; i++) {
        redirections[i] = simpleCommand->redirections[i];
        redirections[i].filename = expandWord(redirections[i].filename);
    }

    const char* command = arguments[0];
    // Special built-ins
    if (strcmp(command, "exit") == 0) {
        exit(0);
    }

    // Regular built-ins
    if (strcmp(command, "cd") == 0) {
        int result = cd(argc, arguments);
        free(arguments);
        free(redirections);
        return result;
    }

    pid_t pid = fork();

    if (pid < 0) {
        warn("fork");
        free(arguments);
        free(redirections);
        return 126;
    } else if (pid == 0) {
        if (!performRedirections(redirections,
                simpleCommand->numRedirections)) {
            _Exit(126);
        }

        if (!strchr(command, '/')) {
            command = getExecutablePath(command);
        }

        if (command) {
            execv(command, arguments);
            warn("execv: '%s'", command);
            _Exit(126);
        } else {
            warnx("'%s': Command not found", arguments[0]);
            _Exit(127);
        }
    } else {
        free(arguments);
        free(redirections);

        int status;
        if (waitpid(pid, &status, 0) < 0) {
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
        if (!buffer) {
            warn("malloc");
            _Exit(126);
        }

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

static bool performRedirections(struct Redirection* redirections,
        size_t numRedirections) {
    for (size_t i = 0; i < numRedirections; i++) {
        struct Redirection redirection = redirections[i];
        int fd;

        if (redirection.filenameIsFd) {
            char* tail;
            fd = strtol(redirection.filename, &tail, 10);
            if (*tail) {
                errno = EBADF;
                warn("'%s'", redirection.filename);
                return false;
            }
        } else {
            fd = open(redirection.filename, redirection.flags, 0666);
            if (fd < 0) {
                warn("open: '%s'", redirection.filename);
                return false;
            }
        }

        if (dup2(fd, redirection.fd) < 0) {
            warn("dup2: '%s'", redirection.filename);
            return false;
        }
        if (!redirection.filenameIsFd) {
            close(fd);
        }
    }

    return true;
}
