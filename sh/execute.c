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

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "builtins.h"
#include "execute.h"
#include "expand.h"
#include "sh.h"

static int executePipeline(struct Pipeline* pipeline);
static int executeSimpleCommand(struct SimpleCommand* simpleCommand,
        bool subshell);
static noreturn void executeUtility(char** arguments,
        struct Redirection* redirections, size_t numRedirections);
static int forkAndExecuteUtility(char** arguments,
        struct Redirection* redirections, size_t numRedirections);
static const char* getExecutablePath(const char* command);
static bool performRedirections(struct Redirection* redirections,
        size_t numRedirections);
static int waitForCommand(pid_t pid);

int execute(struct CompleteCommand* command) {
    return executePipeline(&command->pipeline);
}

static int executePipeline(struct Pipeline* pipeline) {
    int inputFd;

    for (size_t i = 0; i < pipeline->numCommands - 1; i++) {
        int pipeFds[2];
        if (pipe(pipeFds) < 0) err(1, "pipe");

        pid_t pid = fork();

        if (pid < 0) {
            err(1, "fork");
        } else if (pid == 0) {
            close(pipeFds[0]);

            if (i > 0) {
                if (!moveFd(inputFd, 0)) {
                    warn("cannot move file descriptor");
                    _Exit(126);
                }
            }

            if (!moveFd(pipeFds[1], 1)) {
                warn("cannot move file descriptor");
                _Exit(126);
            }

            executeSimpleCommand(&pipeline->commands[i], true);
            assert(false); // This should be unreachable.
        } else {
            close(pipeFds[1]);
            if (i > 0) {
                close(inputFd);
            }

            inputFd = pipeFds[0];
        }
    }

    if (pipeline->numCommands > 1) {
        pid_t pid = fork();

        if (pid < 0) {
            err(1, "fork");
        } else if (pid == 0) {
            if (!moveFd(inputFd, 0)) {
                warn("cannot move file descriptor");
                _Exit(126);
            }

            executeSimpleCommand(
                    &pipeline->commands[pipeline->numCommands - 1], true);
            assert(false); // This should be unreachable.
        } else {
            assert(inputFd != 0);
            close(inputFd);

            int exitStatus = waitForCommand(pid);

            for (size_t i = 0; i < pipeline->numCommands - 1; i++) {
                // Wait for all other commands of the pipeline.
                int status;
                wait(&status);
            }
            if (pipeline->bang) return !exitStatus;
            return exitStatus;
        }
    }

    return executeSimpleCommand(&pipeline->commands[0], false);
}

static int executeSimpleCommand(struct SimpleCommand* simpleCommand,
        bool subshell) {
    int argc = simpleCommand->numWords;

    char** arguments = malloc((argc + 1) * sizeof(char*));
    if (!arguments) err(1, "malloc");
    for (int i = 0; i < argc; i++) {
        arguments[i] = expandWord(simpleCommand->words[i]);
    }

    arguments[argc] = NULL;

    size_t numRedirections = simpleCommand->numRedirections;
    struct Redirection* redirections = malloc(numRedirections *
            sizeof(struct Redirection));
    if (!redirections) err(1, "malloc");
    for (size_t i = 0; i < numRedirections; i++) {
        redirections[i] = simpleCommand->redirections[i];
        redirections[i].filename = expandWord(redirections[i].filename);
    }

    const char* command = arguments[0];
    if (!command) {
        assert(numRedirections > 0);
    // Special built-ins
    } else if (strcmp(command, "exit") == 0) {
        exit(0);
    // Regular built-ins
    } else if (strcmp(command, "cd") == 0) {
        int result = cd(argc, arguments);
        free(arguments);
        free(redirections);
        if (subshell) {
            exit(result);
        }
        return result;
    }

    if (subshell) {
        executeUtility(arguments, redirections, numRedirections);
    } else {
        int result = forkAndExecuteUtility(arguments, redirections,
                numRedirections);
        free(arguments);
        free(redirections);
        return result;
    }
}

static noreturn void executeUtility(char** arguments,
        struct Redirection* redirections, size_t numRedirections) {
    const char* command = arguments[0];
    if (!performRedirections(redirections, numRedirections)) {
        _Exit(126);
    }

    if (!command) _Exit(0);

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
}

static int forkAndExecuteUtility(char** arguments,
        struct Redirection* redirections, size_t numRedirections) {
    pid_t pid = fork();

    if (pid < 0) {
        err(1, "fork");
    } else if (pid == 0) {
        executeUtility(arguments, redirections, numRedirections);
    } else {
        return waitForCommand(pid);
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
        if (!redirection.filenameIsFd && fd != redirection.fd) {
            close(fd);
        }
    }

    return true;
}

static int waitForCommand(pid_t pid) {
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
