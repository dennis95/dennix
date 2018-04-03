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

/* libc/src/stdlib/canonicalize_file_name.c
 * Canonicalizes the name of a file.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char* getEntryName(DIR* dir, dev_t dev, ino_t ino) {
    struct dirent* dirent = readdir(dir);
    while (dirent) {
        if (dirent->d_dev == dev && dirent->d_ino == ino) {
            return dirent->d_name;
        }

        dirent = readdir(dir);
    }

    return NULL;
}

char* canonicalize_file_name(const char* path) {
    if (!path) {
        errno = EINVAL;
        return NULL;
    }

    struct stat st;
    if (stat(path, &st) < 0) return NULL;
    int fd;
    if (S_ISDIR(st.st_mode)) {
        int currentFd = open(path, O_SEARCH | O_CLOEXEC | O_DIRECTORY);
        if (currentFd < 0) return NULL;
        fd = openat(currentFd, "..", O_SEARCH | O_CLOEXEC | O_DIRECTORY);
        close(currentFd);
    } else {
        char* pathCopy = strdup(path);
        if (!pathCopy) return NULL;
        char* slash = strrchr(pathCopy, '/');
        while (slash && !slash[1]) {
            *slash = '\0';
            slash = strrchr(pathCopy, '/');
        }

        if (slash) {
            slash[1] = '\0';
            fd = open(pathCopy, O_SEARCH | O_CLOEXEC | O_DIRECTORY);
        } else {
            fd = open(".", O_SEARCH | O_CLOEXEC | O_DIRECTORY);
        }
        free(pathCopy);
    }

    if (fd < 0) return NULL;

    char* name = malloc(1);
    if (!name) return NULL;
    *name = '\0';
    size_t length = 0;

    while (1) {
        DIR* dir = fdopendir(fd);
        if (!dir) {
            close(fd);
            free(name);
            return NULL;
        }
        char* filename = getEntryName(dir, st.st_dev, st.st_ino);
        if (!filename) {
            closedir(dir);
            free(name);
            errno = ENOENT;
            return NULL;
        }

        if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
            closedir(dir);
            if (!*name) {
                free(name);
                return strdup("/");
            }
            return name;
        }

        length += strlen(filename) + 1;
        char* newName = malloc(length + 1);
        if (!newName) {
            closedir(dir);
            free(name);
            return NULL;
        }

        stpcpy(stpcpy(stpcpy(newName, "/"), filename), name);
        free(name);
        name = newName;

        if (fstat(fd, &st) < 0) {
            closedir(dir);
            free(name);
            return NULL;
        }

        fd = openat(fd, "..", O_SEARCH | O_CLOEXEC | O_DIRECTORY);
        closedir(dir);
        if (fd < 0) {
            free(name);
            return NULL;
        }
    }
}
