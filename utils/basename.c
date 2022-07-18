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

/* utils/basename.c
 * Extract filename from path.
 */

#include "utils.h"
#include <err.h>
#include <getopt.h>
#include <libgen.h>
#include <string.h>

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
            return help(argv[0], "STRING [SUFFIX]\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case '?':
            return 1;
        }
    }

    if (optind >= argc) errx(1, "missing operand");
    if (optind + 2 < argc) errx(1, "extra operand '%s'", argv[optind + 2]);

    const char* base = basename(argv[optind]);
    size_t baseLength = strlen(base);

    if (optind + 1 < argc) {
        const char* suffix = argv[optind + 1];
        size_t suffixLength = strlen(suffix);
        if (baseLength > suffixLength) {
            size_t length = baseLength - suffixLength;
            if (strcmp(base + length, suffix) == 0) {
                baseLength = length;
            }
        }
    }

    fwrite(base, 1, baseLength, stdout);
    fputc('\n', stdout);
}
