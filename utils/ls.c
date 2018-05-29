/* Copyright (c) 2016, 2017, 2018 Dennis Wölfing
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

/* utils/ls.c
 * Lists directory contents.
 */

#include "utils.h"
#include <dirent.h>
#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int filterDot(const struct dirent* dirent);
static void listDirectory(const char* path);
static void printEntriesGrid(struct dirent** list, int numEntries);
static void printEntriesOneline(struct dirent** list, int numEntries);

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "help", no_argument, 0, '?' },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    int c;
    while ((c = getopt_long(argc, argv, "?", longopts, NULL)) != -1) {
        switch (c) {
        case 1:
            return version(argv[0]);
        case '?':
            return help(argv[0], "[OPTIONS] [FILE...]\n"
                    "  -?, --help               display this help\n"
                    "      --version            display version info");
        }
    }

    if (optind >= argc) {
        listDirectory(".");
    }

    for (int i = optind; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) < 0) {
            warn("stat: '%s'", argv[i]);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            listDirectory(argv[i]);
        } else {
            puts(argv[i]);
        }
    }
}

static int filterDot(const struct dirent* dirent) {
    if (dirent->d_name[0] == '.') {
        return 0;
    }
    return 1;
}

static void listDirectory(const char* path) {
    struct dirent** list;
    int numEntries = scandir(path, &list, filterDot, alphasort);
    if (numEntries < 0) {
        warn("scandir: '%s'", path);
        return;
    }

    if (isatty(1)) {
        printEntriesGrid(list, numEntries);
    } else {
        printEntriesOneline(list, numEntries);
    }

    for (int i = 0; i < numEntries; i++) {
        free(list[i]);
    }
    free(list);
}

static void printEntriesGrid(struct dirent** list, int numEntries) {
    static const size_t lineWidth = 80;
    size_t maxLength = 0;

    for (int i = 0; i < numEntries; i++) {
        size_t length = strlen(list[i]->d_name);
        if (length > maxLength) {
            maxLength = length;
        }
    }

    size_t columns = (lineWidth + 1) / (maxLength + 1);
    if (columns == 0) {
        columns = 1;
    }

    size_t lines = (numEntries + columns - 1) / columns;

    size_t columnWidth = lineWidth / columns;

    for (size_t line = 0; line < lines; line++) {
        for (size_t column = 0; column < columns; column++) {
            size_t index = column * lines + line;
            if (index >= (size_t) numEntries) {
                putchar('\n');
                break;
            }

            struct dirent* dirent = list[index];
            size_t length = strlen(dirent->d_name);
            fputs(dirent->d_name, stdout);

            if (column == columns - 1) {
                putchar('\n');
            } else {
                for (size_t i = length; i < columnWidth; i++) {
                    putchar(' ');
                }
            }
        }
    }
}

static void printEntriesOneline(struct dirent** list, int numEntries) {
    for (int i = 0; i < numEntries; i++) {
        puts(list[i]->d_name);
    }
}
