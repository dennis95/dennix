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

/* utils/date.c
 * Prints date and time.
 */

#include "utils.h"
#include <err.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const char* const defaultFormat = "%a %b %e %H:%M:%S %Z %Y";
static const char* const rfc5322Format = "%a, %d %b %Y %H:%M:%S %z";

static char buffer[4096];

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "rfc-email", no_argument, 0, 'R' },
        { "utc", no_argument, 0, 'u' },
        { "universal", no_argument, 0, 'u' },
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    bool rfc5322 = false;
    bool utc = false;

    int c;
    while ((c = getopt_long(argc, argv, "Ru", longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "[OPTIONS] [+FORMAT]\n"
                    "  -R, --rfc-email          RFC 5322 format\n"
                    "  -u, --utc, --universal   print UTC\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case 'R':
            rfc5322 = true;
            break;
        case 'u':
            utc = true;
            break;
        case '?':
            return 1;
        }
    }

    if (optind + 1 < argc) {
        errx(1, "extra operand '%s'", argv[optind + 1]);
    }

    if (utc && setenv("TZ", "UTC0", 1) < 0) {
        err(1, "setenv");
    }

    const char* format;

    if (optind < argc) {
        if (*argv[optind] != '+') {
            errx(1, "invalid operand '%s'", argv[optind]);
        }
        if (rfc5322) errx(1, "multiple output formats specified");
        format = argv[optind] + 1;
    } else if (rfc5322) {
        format = rfc5322Format;
    } else {
        format = defaultFormat;
    }

    time_t t = time(NULL);
    struct tm* tm = localtime(&t);

    if (strftime(buffer, sizeof(buffer), format, tm) == 0) {
        // There is no portable way to distinguish empty result from an error.
        putchar('\n');
    } else {
        puts(buffer);
    }
}
