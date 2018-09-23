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

/* utils/cp.c
 * Copies files.
 */

#include "utils.h"
#include <dirent.h>
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

static int status;

static void copyFile(int sourceFd, const char* sourcePath, int destFd,
        const char* destPath);
static void copy(int sourceFd, const char* sourceName, const char* sourcePath,
        int destFd, const char* destName, const char* destPath, bool force,
        bool prompt, bool recursive);

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "force", no_argument, 0, 'f' },
        { "interactive", no_argument, 0, 'i' },
        { "recursive", no_argument, 0, 'R' },
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    bool force = false;
    bool prompt = false;
    bool recursive = false;
    int c;
    while ((c = getopt_long(argc, argv, "fiRr", longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "[OPTIONS] SOURCE... DESTINATION\n"
                    "  -f, --force              force copy\n"
                    "  -i, --interactive        prompt before overwrite\n"
                    "  -R, -r, --recursive      recursively copy directories\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case 'f':
            force = true;
            prompt = false;
            break;
        case 'i':
            force = false;
            prompt = true;
            break;
        case 'r':
        case 'R':
            recursive = true;
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
        if (statResult < 0 && errno != ENOENT) {
            err(1, "stat: '%s'", destination);
        } else if ((statResult < 0 && errno == ENOENT) ||
                !S_ISDIR(destSt.st_mode)) {
            copy(AT_FDCWD, argv[optind], argv[optind], AT_FDCWD, destination,
                    destination, force, prompt, recursive);
            return status;
        }
    }

    int destFd = open(destination, O_SEARCH | O_DIRECTORY);
    if (destFd < 0) err(1, "open: '%s'", destination);
    for (int i = optind; i < argc - 1; i++) {
        const char* source = argv[i];
        char* sourceCopy = strdup(source);
        if (!sourceCopy) err(1, "strdup");
        char* destName = basename(sourceCopy);
        if (strcmp(destName, "/") == 0) {
            destName = ".";
        }
        char* destPath = malloc(strlen(destination) + strlen(destName) + 2);
        if (!destPath) err(1, "malloc");
        stpcpy(stpcpy(stpcpy(destPath, destination), "/"), destName);
        copy(AT_FDCWD, source, source, destFd, destName, destPath, force,
                prompt, recursive);
        free(sourceCopy);
        free(destPath);
    }
}

static bool getConfirmation(void) {
    char* buffer = NULL;
    size_t size = 0;
    if (getline(&buffer, &size, stdin) <= 0) return false;
    bool result = (*buffer == 'y' || *buffer == 'Y');
    free(buffer);
    return result;
}

static void copyFile(int sourceFd, const char* sourcePath, int destFd,
        const char* destPath) {
    while (true) {
        char buffer[1024];
        ssize_t bytesAvailable = read(sourceFd, buffer, sizeof(buffer));
        if (bytesAvailable < 0) {
            warn("read: '%s'", sourcePath);
            status = 1;
            return;
        } else if (bytesAvailable == 0) {
            return;
        }
        while (bytesAvailable) {
            ssize_t bytesWritten = write(destFd, buffer, bytesAvailable);
            if (bytesWritten < 0) {
                warn("write: '%s'", destPath);
                status = 1;
                return;
            }
            bytesAvailable -= bytesWritten;
        }
    }
}

static void copy(int sourceFd, const char* sourceName, const char* sourcePath,
        int destFd, const char* destName, const char* destPath, bool force,
        bool prompt, bool recursive) {
    struct stat sourceSt, destSt;
    if (fstatat(sourceFd, sourceName, &sourceSt, 0) < 0) {
        warn("stat: '%s'", sourcePath);
        status = 1;
        return;
    }
    bool destExists = true;
    if (fstatat(destFd, destName, &destSt, 0) < 0) {
        if (errno != ENOENT) {
            warn("stat: '%s'", destPath);
            status = 1;
            return;
        }
        destExists = false;
    }
    if (destExists && sourceSt.st_dev == destSt.st_dev &&
            sourceSt.st_ino == destSt.st_ino) {
        warnx("'%s' and '%s' are the same file", sourcePath, destPath);
        status = 1;
        return;
    }
    if (S_ISDIR(sourceSt.st_mode)) {
        if (!recursive) {
            warnx("omitting directory '%s' because -R is not specified",
                    sourcePath);
            status = 1;
            return;
        }
        if (destExists && !S_ISDIR(destSt.st_mode)) {
            warnx("cannot overwrite '%s' with directory '%s'", destPath,
                    sourcePath);
            status = 1;
            return;
        }
        if (!destExists) {
            if (mkdirat(destFd, destName, S_IRWXU) < 0) {
                warn("mkdir: '%s'", destPath);
                status = 1;
                return;
            }
            if (fstatat(destFd, destName, &destSt, 0) < 0) {
                warn("stat: '%s'", destPath);
                status = 1;
                return;
            }
            destExists = true;
        }

        int newSourceFd = openat(sourceFd, sourceName, O_SEARCH | O_DIRECTORY);
        if (newSourceFd < 0) {
            warn("open: '%s'", sourcePath);
            status = 1;
            return;
        }
        DIR* dir = fdopendir(newSourceFd);
        if (!dir) {
            warn("fdopendir: '%s'", sourcePath);
            close(newSourceFd);
            status = 1;
            return;
        }
        int newDestFd = openat(destFd, destName, O_SEARCH | O_DIRECTORY);
        if (newDestFd < 0) {
            warn("open: '%s'", destPath);
            closedir(dir);
            status = 1;
            return;
        }
        struct dirent* dirent = readdir(dir);
        while (dirent) {
            if (strcmp(dirent->d_name, ".") == 0 ||
                    strcmp(dirent->d_name, "..") == 0) {
                dirent = readdir(dir);
                continue;
            }

            char* newSourcePath = malloc(strlen(sourcePath) +
                    strlen(dirent->d_name) + 2);
            if (!newSourcePath) err(1, "malloc");
            stpcpy(stpcpy(stpcpy(newSourcePath, sourcePath), "/"),
                    dirent->d_name);
            char* newDestPath = malloc(strlen(destPath) +
                    strlen(dirent->d_name) + 2);
            if (!newDestPath) err(1, "malloc");
            stpcpy(stpcpy(stpcpy(newDestPath, destPath), "/"), dirent->d_name);

            if (destExists && dirent->d_dev == destSt.st_dev &&
                    dirent->d_ino == destSt.st_ino) {
                warnx("cannot copy directory '%s' into itself '%s'",
                        newSourcePath, destPath);
            } else {
                copy(newSourceFd, dirent->d_name, newSourcePath, newDestFd,
                    dirent->d_name, newDestPath, force, prompt, recursive);
            }

            free(newSourcePath);
            free(newDestPath);

            dirent = readdir(dir);
        }

        closedir(dir);
        close(newDestFd);
    } else if (S_ISREG(sourceSt.st_mode)) {
        int newDestFd;
        if (destExists) {
            if (prompt) {
                fprintf(stderr, "%s: overwrite '%s'? ",
                        program_invocation_short_name, destPath);
                if (!getConfirmation()) return;
            }

            newDestFd = openat(destFd, destName, O_WRONLY | O_TRUNC);
            if (newDestFd < 0) {
                if (force) {
                    if (unlinkat(destFd, destName, 0) < 0) {
                        warn("unlinkat: '%s'", destPath);
                        status = 1;
                        return;
                    }
                } else {
                    warn("open: '%s'", destPath);
                    status = 1;
                    return;
                }
            } else {
                destExists = true;
            }
        }
        if (!destExists) {
            newDestFd = openat(destFd, destName, O_WRONLY | O_CREAT,
                    sourceSt.st_mode & 0777);
            if (newDestFd < 0) {
                warn("open: '%s'", destPath);
                status = 1;
                return;
            }
        }
        int newSourceFd = openat(sourceFd, sourceName, O_RDONLY);
        if (newSourceFd < 0) {
            warn("open: '%s'", sourcePath);
            close(newDestFd);
            status = 1;
            return;
        }

        copyFile(newSourceFd, sourcePath, newDestFd, destPath);
        close(newDestFd);
        close(newSourceFd);
    } else {
        warnx("unsupported file type: '%s'", sourcePath);
        status = 1;
    }
}
