/* Copyright (c) 2019 Dennis Wölfing
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

/* libc/src/stdlib/system.c
 * Execute a command.
 */

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int system(const char* command) {
    if (!command) {
        return access("/bin/sh", X_OK) == 0;
    }

    struct sigaction ignore;
    ignore.sa_handler = SIG_IGN;
    ignore.sa_flags = 0;

    struct sigaction sigint;
    struct sigaction sigquit;
    sigaction(SIGINT, &ignore, &sigint);
    sigaction(SIGQUIT, &ignore, &sigquit);

    sigset_t maskSigchld;
    sigemptyset(&maskSigchld);
    sigaddset(&maskSigchld, SIGCHLD);
    sigset_t oldMask;
    sigprocmask(SIG_BLOCK, &maskSigchld, &oldMask);

    int status;

    pid_t pid = fork();
    if (pid < 0) {
        status = -1;
    } else if (pid == 0) {
        sigaction(SIGINT, &sigint, NULL);
        sigaction(SIGQUIT, &sigquit, NULL);
        sigprocmask(SIG_SETMASK, &oldMask, NULL);

        execl("/bin/sh", "sh", "-c", command, NULL);
        _Exit(127);
    } else {
        while (waitpid(pid, &status, 0) < 0) {
            if (errno != EINTR) {
                status = -1;
                break;
            }
        }
    }

    sigaction(SIGINT, &sigint, NULL);
    sigaction(SIGQUIT, &sigquit, NULL);
    sigprocmask(SIG_SETMASK, &oldMask, NULL);

    return status;
}
