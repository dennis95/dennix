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

/* utils/rmdir.c
 * Remove directories.
 */

#include "utils.h"
#include <err.h>
#include <getopt.h>
#include <libgen.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

static bool parents;

static bool containsMoreThanOnePathnameComponent(const char* dir);
static bool handleOperand(char* dir);

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "parents", no_argument, 0, 'p' },
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    int c;
    while ((c = getopt_long(argc, argv, "p", longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "[OPTIONS] DIR...\n"
                    "  -p, --parents            remove parent directories\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case 'p':
            parents = true;
            break;
        case '?':
            return 1;
        }
    }

    if (optind >= argc) errx(1, "missing operand");

    bool success = true;
    for (int i = optind; i < argc; i++) {
        success &= handleOperand(argv[i]);
    }

    return success ? 0 : 1;
}

static bool containsMoreThanOnePathnameComponent(const char* dir) {
    const char* slash = strchr(dir, '/');
    if (!slash) return false;

    do {
        slash++;
    } while (*slash == '/');

    return *slash != '\0';
}

static bool handleOperand(char* dir) {
    if (rmdir(dir) < 0) {
        warn("cannot remove '%s'", dir);
        return false;
    }

    if (parents) {
        while (containsMoreThanOnePathnameComponent(dir)) {
            dir = dirname(dir);
            if (rmdir(dir) < 0) {
                warn("cannot remove '%s'", dir);
                return false;
            }
        }
    }

    return true;
}
