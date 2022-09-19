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

/* utils/kill.c
 * Send a signal to a process.
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    bool list = false;
    char* signalName = NULL;

    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') break;
        if (argv[i][1] == '-' && argv[i][2] == '\0') {
            i++;
            break;
        }

        for (size_t j = 1; argv[i][j]; j++) {
            if (argv[i][j] == 'l') {
                list = true;
            } else if (argv[i][j] == 's') {
                char* argument;
                if (argv[i][j + 1]) {
                    argument = argv[i] + j + 1;
                } else {
                    argument = argv[++i];
                    if (!argument) {
                        errx(1, "option requires a argument -- 's'");
                    }
                }

                signalName = argument;
                break;
            } else {
                signalName = argv[i] + j;
                break;
            }
        }
    }

    int status = 0;

    if (list) {
        if (argv[i]) {
            while (argv[i]) {
                char* end;
                errno = 0;
                long value = strtol(argv[i], &end, 10);
                if (errno || *end) {
                    warnx("invalid exit status '%s'", argv[i]);
                    i++;
                    status = 1;
                    continue;
                }

                if (value > 128) {
                    value -= 128;
                }

                char buffer[SIG2STR_MAX];
                if (sig2str(value, buffer) != 0) {
                    warnx("invalid exit status '%s'", argv[i]);
                    status = 1;
                } else {
                    puts(buffer);
                }
                i++;
            }
            return status;
        }

        for (int i = 1; i < NSIG; i++) {
            char buffer[SIG2STR_MAX];
            sig2str(i, buffer);
            printf("%s%c", buffer, i == NSIG - 1 ? '\n' : ' ');
        }
        return 0;
    }

    int signum;
    if (!signalName) {
        signum = SIGTERM;
    } else if (strcmp(signalName, "0") == 0) {
        signum = 0;
    } else {
        // Convert the name to upper case.
        for (size_t j = 0; signalName[j]; j++) {
            signalName[j] = toupper((unsigned char) signalName[j]);
        }

        if (str2sig(signalName, &signum) != 0) {
            errx(1, "invalid signal '%s'", signalName);
        }
    }

    if (!argv[i]) errx(1, "missing operand");

    while (argv[i]) {
        char* end;
        errno = 0;
        long long value = strtoll(argv[i], &end, 10);
        if (errno || *end || (pid_t) value != value) {
            warnx("invalid pid '%s'", argv[i]);
            i++;
            status = 1;
            continue;
        }

        if (kill(value, signum) < 0) {
            warn("failed to send signal to %lld", value);
            status = 1;
        }
        i++;
    }

    return status;
}
