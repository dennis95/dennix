/* Copyright (c) 2017 Dennis Wölfing
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

/* utils/init.c
 * System initialization.
 */

#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char* argv[]) {
    (void) argc; (void) argv;

    if (getpid() != 1) errx(1, "PID is not 1");

    if (setenv("PATH", "/bin:/sbin", 1) < 0) err(1, "setenv");

    pid_t childPid = fork();
    if (childPid < 0) err(1, "fork");

    if (childPid == 0) {
        const char* args[] = { "sh", NULL };
        execv("/bin/sh", (char**) args);
        err(1, "execv: '/bin/sh'");
    }

    while (true) {
        // Wait for any orphaned processes.
        int status;
        wait(&status);
    }
}
