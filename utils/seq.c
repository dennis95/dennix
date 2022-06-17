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

/* utils/seq.c
 * Print a sequence of numbers.
 */

#include "utils.h"
#include <err.h>
#include <errno.h>
#include <getopt.h>

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "separator", required_argument, 0, 's' },
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    const char* separator = "\n";
    const char* shortopts = "s:0123456789";
    int c;
    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "[OPTIONS] [START [STEP]] END\n"
                    "  -s, --separator=SEP      separate by SEP\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case 's':
            separator = optarg;
            break;
        case '0': case '1': case '2': case '3': case '4': case '5': case '6':
        case '7': case '8': case '9':
            if (optind > 1) {
                const char* prev = argv[optind - 1];
                if (prev[0] == '-' && prev[1] == c) {
                    optind--;
                }
            }
            if (!argv[optind] || argv[optind][0] != '-' ||
                    argv[optind][1] != c) {
                errx(1, "invalid option -- '%c'", c);
            }
            goto options_done;
        case '?':
            return 1;
        }
    }
options_done:;

    int numOperands = argc - optind;
    if (numOperands <= 0) errx(1, "missing operand");
    if (numOperands >= 4) errx(1, "extra operand");

    long long start = 1;
    long long step = 1;
    long long end;

    if (numOperands >= 2) {
        char* endstr;
        errno = 0;
        start = strtoll(argv[optind], &endstr, 10);
        if (errno || *endstr) {
            errx(1, "invalid number '%s'", argv[optind]);
        }
        optind++;
    }

    if (numOperands == 3) {
        char* endstr;
        errno = 0;
        step = strtoll(argv[optind], &endstr, 10);
        if (errno || step == 0 || *endstr) {
            errx(1, "invalid number '%s'", argv[optind]);
        }
        optind++;
    }

    char* endstr;
    errno = 0;
    end = strtoll(argv[optind], &endstr, 10);
    if (errno || *endstr) {
        errx(1, "invalid number '%s'", argv[optind]);
    }

    if ((end < start && step > 0) || (end > start && step < 0)) {
        return 0;
    }

    for (long long i = start; step > 0 ? i <= end : end <= i; i += step) {
        if (i != start) {
            fputs(separator, stdout);
        }
        printf("%lld", i);
    }
    fputc('\n', stdout);
}
