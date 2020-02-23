/* Copyright (c) 2017, 2019, 2020 Dennis Wölfing
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

#include <devctl.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dennix/display.h>

int main(int argc, char* argv[]) {
    (void) argc; (void) argv;

    if (getpid() != 1) errx(1, "PID is not 1");

    if (setenv("PATH", "/bin:/sbin", 1) < 0) err(1, "setenv");

    const char* term = "dennix-16color";
    int fd = open("/dev/display", O_RDONLY);
    if (fd >= 0) {
        struct display_resolution res;
        // Check whether we are running in graphics mode. In text mode this call
        // fails with ENOTSUP.
        if (posix_devctl(fd, DISPLAY_GET_RESOLUTION, &res, sizeof(res),
                NULL) == 0) {
            term = "dennix";
        }
        close(fd);
    }

    if (setenv("TERM", term, 1) < 0) err(1, "setenv");

    pid_t childPid = fork();
    if (childPid < 0) err(1, "fork");

    if (childPid == 0) {
        setpgid(0, 0);
        tcsetpgrp(0, getpid());

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
