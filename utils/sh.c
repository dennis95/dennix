/* Copyright (c) 2016, Dennis Wölfing
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

/* utils/sh.c
 * The shell.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int executeCommand(char* arguments[]);
static const char* getExecutablePath(const char* command);

int main(int argc, char* argv[]) {
    (void) argc; (void) argv;

    while (true) {
        fputs("$ ", stderr);
        // TODO: Commands might be longer than 80 characters.
        char buffer[81];
        fgets(buffer, sizeof(buffer), stdin);

        size_t length = strlen(buffer);
        if (buffer[length - 1] == '\n') {
            buffer[length - 1] = '\0';
        }

        // Count the arguments
        size_t argumentCount = 1;
        for (size_t i = 0; buffer[i]; i++) {
            if (buffer[i] == ' ') {
                argumentCount++;
            }
        }

        // Create the argument list
        char** arguments = malloc((argumentCount + 1) * sizeof(char*));
        char* str = strtok(buffer, " ");
        for (size_t i = 0; i < argumentCount; i++) {
            arguments[i] = str;
            str = strtok(NULL, " ");
        }
        arguments[argumentCount] = NULL;

        executeCommand(arguments);
        free(arguments);
    }
}

static int executeCommand(char* arguments[]) {
    const char* command = arguments[0];
    // Special built-ins
    if (strcmp(command, "exit") == 0) {
        exit(0);
    }

    pid_t pid = fork();

    if (pid < 0) {
        fputs("fork() failed\n", stderr);
        return -1;
    } else if (pid == 0) {
        if (!strchr(command, '/')) {
            command = getExecutablePath(command);
        }

        if (command) {
            execv(command, arguments);
        }

        fputs("Command not found\n", stderr);
        _Exit(127);
    } else {
        int status;
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }
}

static const char* getExecutablePath(const char* command) {
    size_t commandLength = strlen(command);
    const char* path = getenv("PATH");

    while (*path) {
        size_t length = strcspn(path, ":");
        char* buffer = malloc(commandLength + length + 2);

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
