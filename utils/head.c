/* Copyright (c) 2018, 2019 Dennis Wölfing
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

/* utils/head.c
 * Copy first part of a file.
 */

#ifndef TAIL
#  define TAIL 0
#endif
#define HEAD !TAIL

#include "utils.h"
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BYTES, LINES };
static int unit = LINES;
static off_t amount = 10;
static bool fromBeginning = HEAD;

static bool headOrTail(FILE* file);
static void headOrTailBackwardNoSeek(FILE* file);
static off_t parseAmount(const char* arg);

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "bytes", required_argument, 0, 'c' },
        { "lines", required_argument, 0, 'n' },
        { "quiet", no_argument, 0, 'q' },
        { "silent", no_argument, 0, 'q' },
        { "verbose", no_argument, 0, 'v' },
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    bool quiet = false;
    bool verbose = false;

    int c;
    while ((c = getopt_long(argc, argv, "c:n:qv", longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "[OPTIONS] [FILE...]\n"
                    "  -c, --bytes=NUMBER       count bytes\n"
                    "  -n, --lines=NUMBER       count lines\n"
                    "  -q, --quiet, --silent    never print file name\n"
                    "  -v, --verbose            always print file name\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case 'c':
            unit = BYTES;
            amount = parseAmount(optarg);
            break;
        case 'n':
            unit = LINES;
            amount = parseAmount(optarg);
            break;
        case 'q':
            quiet = true;
            verbose = false;
            break;
        case 'v':
            verbose = true;
            quiet = false;
            break;
        case '?':
            return 1;
        }
    }

    bool success = true;

    for (int i = optind; i < argc; i++) {
        FILE* file;
        if (strcmp(argv[i], "-") == 0) {
            file = stdin;
        } else {
            file = fopen(argv[i], "r");
            if (!file) {
                warn("open: '%s'", argv[i]);
                success = false;
                continue;
            }
        }

        if (verbose || (!quiet && optind + 1 < argc)) {
            if (i > optind) putchar('\n');
            printf("==> %s <==\n", file == stdin ? "standard input" : argv[i]);
        }

        success &= headOrTail(file);

        if (file != stdin) {
            fclose(file);
        }
    }

    if (optind >= argc) {
        if (verbose) {
            puts("==> standard input <==");
        }
        success = headOrTail(stdin);
    }

    return success ? 0 : 1;
}

static bool headOrTail(FILE* file) {
    off_t initialPosition = ftello(file);
    bool isSeekable = initialPosition >= 0;

    if (fromBeginning) {
#if TAIL
        if (isSeekable && unit == BYTES) {
            if (fseeko(file, amount ? amount - 1 : 0, SEEK_CUR) < 0) {
                warn("fseeko");
                return false;
            }
        } else
#endif
        {
            off_t count = HEAD ? 0 : 1;
            while (count < amount) {
                int c = fgetc(file);
                if (c == EOF) break;
                if (unit == BYTES || c == '\n') {
                    count++;
                }
#if HEAD
                putchar(c);
#endif
            }
        }

#if TAIL
        while (true) {
            int c = fgetc(file);
            if (c == EOF) break;
            putchar(c);
        }
#endif

    } else {
        if (isSeekable && unit == BYTES) {
            if (fseeko(file, -amount, SEEK_END) < 0 && errno != EINVAL) {
                warn("fseeko");
                return false;
            }

            off_t newPosition = ftello(file);
            if (newPosition < 0) {
                warn("ftello");
                return false;
            }

            if (HEAD || newPosition < initialPosition) {
                if (fseeko(file, initialPosition, SEEK_SET) < 0) {
                    warn("fseeko");
                    return false;
                }
            }
            if (newPosition < initialPosition) {
                newPosition = initialPosition;
            }

#if HEAD
            off_t count = newPosition - initialPosition;
            for (off_t i = 0; i < count; i++)
#else
            while (true)
#endif
            {
                int c = fgetc(file);
                if (c == EOF) break;
                putchar(c);
            }
        } else {
            headOrTailBackwardNoSeek(file);
        }
    }

    return true;
}

static void headOrTailBackwardNoSeek(FILE* file) {
    if (unit == LINES) {
        size_t bufferSize = amount + 1;
        char** buffer = calloc(bufferSize, sizeof(char*));
        if (!buffer) err(1, "calloc");

        size_t index = 0;
        size_t length = 0;

        while (true) {
            size_t size = 0;
            if (getline(&buffer[index], &size, file) < 0) break;
            index = (index + 1) % bufferSize;

            if (buffer[index]) {
#if HEAD
                fputs(buffer[index], stdout);
#endif
                free(buffer[index]);
                buffer[index] = NULL;
            } else {
                length++;
            }
        }

        index = (bufferSize - length + index) % bufferSize;

        while (buffer[index]) {
#if TAIL
            fputs(buffer[index], stdout);
#endif
            free(buffer[index]);
            buffer[index] = NULL;

            index = (index + 1) % bufferSize;
        }

        free(buffer);
    } else {
        unsigned char* buffer = malloc(amount);
        if (!buffer) err(1, "malloc");

        size_t index = 0;
        size_t length = 0;

        while (true) {
            int c = fgetc(file);
            if (c == EOF) break;
#if HEAD
            if (length == (size_t) amount) {
                putchar(buffer[index]);
            }
#endif
            buffer[index] = (unsigned char) c;
            index = (index + 1) % amount;
            if (length < (size_t) amount) {
                length++;
            }
        }

#if TAIL
        index = (amount - length + index) % amount;

        while (length) {
            putchar(buffer[index]);
            index = (index + 1) % amount;
            length--;
        }
#endif

        free(buffer);
    }
}

static off_t parseAmount(const char* argument) {
    const char* arg = argument;
    if (arg[0] == '+') {
        fromBeginning = true;
    } else if (arg[0] == '-') {
        fromBeginning = false;
        arg++;
    } else {
        fromBeginning = HEAD;
    }

    char* end;
    intmax_t amount = strtoimax(arg, &end, 10);
    if (amount < 0 || amount == INTMAX_MAX || *end ||
            (off_t) amount != amount) {
        errx(1, "invalid amount: '%s'", argument);
    }

    return (off_t) amount;
}
