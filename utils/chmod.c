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

/* utils/chmod.c
 * Change file mode.
 */

#include "utils.h"
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static bool changeMode(const char* modeSpec, int dirFd, const char* fileName,
        const char* filePath, bool recursive);
static mode_t getMode(const char* modeSpec, mode_t oldMode);

static mode_t mask;

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "recursive" , no_argument, 0, 'R' },
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    bool recursive = false;

    int c;
    while ((c = getopt_long(argc, argv, "RugorwxX", longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "[OPTIONS] MODE FILE...\n"
                    "  -R, --recursive          recurse through directories\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case 'R':
            recursive = true;
            break;
        case 'u': case 'g': case 'o':
        case 'r': case 'w': case 'x': case 'X':
            // These are not options but can appear in a mode operand beginning
            // with a minus. Stop option parsing and treat the current argument
            // as an operand. If c was the last character in the argument then
            // optind was increased by one. We must decrease it in this case.
            if (optind > 1) {
                const char* prev = argv[optind - 1];
                if (prev[0] == '-' && prev[1] != '-' &&
                        prev[strlen(prev) - 1] == c) {
                    optind--;
                }
            }
            goto options_done;
        case '?':
            return 1;
        }
    }
options_done:
    if (optind >= argc) errx(1, "missing mode operand");
    if (optind + 1 == argc) errx(1, "missing file operand");

    const char* modeSpec = argv[optind++];
    mask = umask(0);

    // Fail early if modeSpec is invalid.
    getMode(modeSpec, 0);

    bool success = true;
    for (int i = optind; i < argc; i++) {
        success &= changeMode(modeSpec, AT_FDCWD, argv[i], argv[i], recursive);
    }
    return success ? 0 : 1;
}

static bool changeMode(const char* modeSpec, int dirFd, const char* fileName,
        const char* filePath, bool recursive) {
    struct stat st;
    if (fstatat(dirFd, fileName, &st, 0) < 0) {
        warn("stat: '%s'", filePath);
        return false;
    }
    mode_t newMode = getMode(modeSpec, st.st_mode);
    if (fchmodat(dirFd, fileName, newMode, 0) < 0) {
        warn("chmod: '%s'", filePath);
        return false;
    }

    bool success = true;
    if (recursive && S_ISDIR(st.st_mode)) {
        int fd = openat(dirFd, fileName, O_RDONLY | O_DIRECTORY);
        if (fd < 0) {
            warn("'%s'", filePath);
            return false;
        }
        DIR* dir = fdopendir(fd);
        if (!dir) err(1, "fdopendir");

        errno = 0;
        struct dirent* dirent = readdir(dir);
        while (dirent) {
            const char* name = dirent->d_name;
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                errno = 0;
                dirent = readdir(dir);
                continue;
            }

            char* path = malloc(strlen(filePath) + strlen(name) + 2);
            if (!path) err(1, "malloc");
            stpcpy(stpcpy(stpcpy(path, filePath), "/"), name);

            // Ignore symlinks when recursing.
            if (fstatat(fd, name, &st, AT_SYMLINK_NOFOLLOW) < 0) {
                warn("lstat: '%s'", path);
                free(path);
                success = false;
                errno = 0;
                dirent = readdir(dir);
                continue;
            }
            if (S_ISLNK(st.st_mode)) {
                free(path);
                errno = 0;
                dirent = readdir(dir);
                continue;
            }

            success &= changeMode(modeSpec, fd, name, path, recursive);
            free(path);

            errno = 0;
            dirent = readdir(dir);
        }

        if (errno != 0) err(1, "readdir");
        closedir(dir);
    }
    return success;
}

#define GET_USER(mode) (((mode) & 0700) >> 6)
#define GET_GROUP(mode) (((mode) & 070) >> 3)
#define GET_OTHER(mode) ((mode) & 07)
#define COPY_MODE(mode) (((mode) << 6) | ((mode) << 3) | (mode))

static mode_t getMode(const char* modeSpec, mode_t oldMode) {
    const char* spec = modeSpec;
    mode_t mode = oldMode & 07777;

    if (*spec >= '0' && *spec <= '9') {
        char* end;
        unsigned long value = strtoul(spec, &end, 8);
        if (value > 07777 || *end) errx(1, "invalid mode: '%s'", modeSpec);
        return (mode_t) value;
    }

    while (true) {
        mode_t who = 0;

        while (*spec == 'u' || *spec == 'g' || *spec == 'o' || *spec == 'a') {
            who |= *spec == 'u' ? 0700 :
                    *spec == 'g' ? 070 :
                    *spec == 'o' ? 07 : 0777;
            spec++;
        }
        if (!who) {
            who = ~mask & 0777;
        }

        do {
            enum op { ADD, REMOVE, SET };
            enum op op = *spec == '+' ? ADD :
                    *spec == '-' ? REMOVE :
                    *spec == '=' ? SET : -1;
            if (op < 0) errx(1, "invalid mode: '%s'", modeSpec);
            spec++;

            mode_t perms;
            if (*spec == 'u' || *spec == 'g' || *spec == 'o') {
                mode_t copy = *spec == 'u' ? GET_USER(mode) :
                        *spec == 'g' ? GET_GROUP(mode) : GET_OTHER(mode);
                perms = COPY_MODE(copy);
                spec++;
            } else {
                perms = 0;
                while (*spec == 'r' || *spec == 'w' || *spec == 'x' ||
                        *spec == 'X') {
                    perms |= *spec == 'r' ? 0444 :
                            *spec == 'w' ? 0222 :
                            *spec == 'x' ? 0111 : 0;
                    if (*spec == 'X' && (S_ISDIR(oldMode) || mode & 0111)) {
                        perms |= 0111;
                    }
                    spec++;
                }
            }

            if (op == ADD) {
                mode |= who & perms;
            } else if (op == REMOVE) {
                mode &= ~(who & perms);
            } else {
                mode = (~who & mode) | (who & perms);
            }
        } while (*spec == '+' || *spec == '-' || *spec == '=');

        if (*spec == '\0') return mode;
        if (*spec != ',') errx(1, "invalid mode: '%s'", modeSpec);
        spec++;
    }
}
