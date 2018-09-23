/* Copyright (c) 2017, 2018 Dennis Wölfing
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

/* utils/uname.c
 * Prints system information.
 */

#include "utils.h"
#include <err.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/utsname.h>

enum {
    PRINT_SYSNAME = 1 << 0,
    PRINT_NODENAME = 1 << 1,
    PRINT_RELEASE = 1 << 2,
    PRINT_VERSION = 1 << 3,
    PRINT_MACHINE = 1 << 4,
    PRINT_ALL = PRINT_SYSNAME | PRINT_NODENAME | PRINT_RELEASE | PRINT_VERSION
            | PRINT_MACHINE
};

bool needSpace = false;

static void printInfo(const char* str) {
    if (needSpace) {
        putchar(' ');
    }
    fputs(str, stdout);
    needSpace = true;
}

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "all", no_argument, 0, 'a' },
        { "kernel-name", no_argument, 0, 's' },
        { "nodename", no_argument, 0, 'n' },
        { "kernel-release", no_argument, 0, 'r' },
        { "kernel-version", no_argument, 0, 'v' },
        { "machine", no_argument, 0, 'm' },
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    int flags = 0;
    int c;
    while ((c = getopt_long(argc, argv, "amnrsv", longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "[OPTIONS]\n"
                    "  -a, --all                print all information\n"
                    "  -s, --kernel-name        print operating system name\n"
                    "  -n, --nodename           print node name\n"
                    "  -r, --kernel-release     print kernel release\n"
                    "  -v, --kernel-version     print kernel version\n"
                    "  -m, --machine            print hardware architecture\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case 'a':
            flags = PRINT_ALL;
            break;
        case 's':
            flags |= PRINT_SYSNAME;
            break;
        case 'n':
            flags |= PRINT_NODENAME;
            break;
        case 'r':
            flags |= PRINT_RELEASE;
            break;
        case 'v':
            flags |= PRINT_VERSION;
            break;
        case 'm':
            flags |= PRINT_MACHINE;
            break;
        case '?':
            return 1;
        }
    }

    if (optind < argc) {
        errx(1, "extra operand '%s'", argv[optind]);
    }

    if (flags == 0) {
        flags = PRINT_SYSNAME;
    }

    struct utsname name;
    uname(&name);

    if (flags & PRINT_SYSNAME) {
        printInfo(name.sysname);
    }
    if (flags & PRINT_NODENAME) {
        printInfo(name.nodename);
    }
    if (flags & PRINT_RELEASE) {
        printInfo(name.release);
    }
    if (flags & PRINT_VERSION) {
        printInfo(name.version);
    }
    if (flags & PRINT_MACHINE) {
        printInfo(name.machine);
    }

    putchar('\n');
}
