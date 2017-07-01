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

/* utils/pwd.c
 * Prints the current working directory.
 */

#include "utils.h"
#include <err.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static bool isAbsolutePath(const char* path);

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "help", no_argument, 0, '?' },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    bool logical = true;
    int c;
    while ((c = getopt_long(argc, argv, "LP?", longopts, NULL)) != -1) {
        switch (c) {
        case 1:
            return version(argv[0]);
        case 'L':
            logical = true;
            break;
        case 'P':
            logical = false;
            break;
        case '?':
            return help(argv[0], "[OPTIONS]\n"
                    "  -L                       print logical path\n"
                    "  -P                       print physical path\n"
                    "  -?, --help               display this help\n"
                    "      --version            display version info");
        }
    }

    if (optind < argc) {
        errx(1, "extra operand '%s'", argv[optind]);
    }

    const char* pwd = getenv("PWD");
    if (logical && pwd && isAbsolutePath(pwd)) {
        puts(pwd);
    } else {
        pwd = getcwd(NULL, 0);
        if (!pwd) err(1, "getcwd");
        puts(pwd);
    }
}

static bool isAbsolutePath(const char* path) {
    if (*path != '/') return false;
    while (*path) {
        if (path[0] == '.' && (path[1] == '/' || path[1] == '\0' ||
                (path[1] == '.' && (path[2] == '/' || path[2] == '\0')))) {
            return false;
        }

        while (*path && *path++ != '/');
    }
    return true;
}
