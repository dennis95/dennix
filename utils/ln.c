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

/* utils/ln.c
 * Creates hard links and symbolic links.
 */

#include "utils.h"
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static bool createLink(const char* target, int dirFd, const char* linkName,
        const char* linkPath, bool force, bool symbolic, bool logical);

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "force", no_argument, 0, 'f' },
        { "logical", no_argument, 0, 'L' },
        { "physical", no_argument, 0, 'P' },
        { "symbolic", no_argument, 0, 's' },
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    bool force = false;
    bool logical = false;
    bool symbolic = false;

    int c;
    while ((c = getopt_long(argc, argv, "fLPs", longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "[OPTIONS] TARGET... LINK\n"
                    "  -f, --force              remove existing files\n"
                    "  -L, --logical            link to symlink target\n"
                    "  -P, --physical           link to symlink itself\n"
                    "  -s, --symbolic           create symbolic link\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case 'f':
            force = true;
            break;
        case 'L':
            logical = true;
            break;
        case 'P':
            logical = false;
            break;
        case 's':
            symbolic = true;
            break;
        case '?':
            return 1;
        }
    }

    if (optind >= argc) errx(1, "missing source operand");
    if (optind == argc - 1) errx(1, "missing destination operand");

    const char* destination = argv[argc - 1];
    if (optind == argc - 2) {
        struct stat destSt;
        int statResult = stat(destination, &destSt);
        if (statResult < 0 && errno != ENOENT && errno != ELOOP) {
            err(1, "stat: '%s'", destination);
        } else if (statResult < 0 || !S_ISDIR(destSt.st_mode)) {
            bool success = createLink(argv[optind], AT_FDCWD, destination,
                    destination, force, symbolic, logical);
            return success ? 0 : 1;
        }
    }

    bool success = true;

    int destFd = open(destination, O_SEARCH | O_DIRECTORY);
    if (destFd < 0) err(1, "open: '%s'", destination);
    for (int i = optind; i < argc - 1; i++) {
        const char* target = argv[i];
        char* targetCopy = strdup(target);
        if (!targetCopy) err(1, "strdup");
        char* linkName = basename(targetCopy);
        if (strcmp(linkName, "/") == 0) {
            linkName = ".";
        }
        char* linkPath = malloc(strlen(destination) + strlen(linkName) + 2);
        if (!linkPath) err(1, "malloc");
        stpcpy(stpcpy(stpcpy(linkPath, destination), "/"), linkName);
        success &= createLink(target, destFd, linkName, linkPath, force,
                symbolic, logical);
        free(targetCopy);
        free(linkPath);
    }
    return success ? 0 : 1;
}

static bool createLink(const char* target, int dirFd, const char* linkName,
        const char* linkPath, bool force, bool symbolic, bool logical) {
    struct stat st;
    bool fileExists = true;
    if (fstatat(dirFd, linkName, &st, 0) < 0 && errno != ELOOP) {
        if (errno != ENOENT) {
            warn("stat: '%s'", linkPath);
            return false;
        }
        fileExists = false;
    }

    if (fileExists) {
        if (!force) {
            errno = EEXIST;
            warn("cannot create link '%s'", linkPath);
            return false;
        }

        // TODO: Check whether the operands refer to the same directory entry
        // and error in that case.

        if (unlinkat(dirFd, linkName, 0) < 0) {
            warn("unlink: '%s'", linkPath);
            return false;
        }
    }

    if (symbolic) {
        if (symlinkat(target, dirFd, linkName) < 0) {
            warn("symlink: '%s'", linkPath);
            return false;
        }
    } else {
        if (linkat(AT_FDCWD, target, dirFd, linkName,
                logical ? AT_SYMLINK_FOLLOW : 0) < 0) {
            warn("link: '%s'", linkPath);
            return false;
        }
    }

    return true;
}
