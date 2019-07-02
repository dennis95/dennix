/* Copyright (c) 2017, 2018, 2019 Dennis Wölfing
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

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

static int openParentDir(const char* path) {
    struct stat st;
    if (lstat(path, &st) < 0) return -1;
    char* parentPath;

    if (S_ISLNK(st.st_mode)) {
        size_t linkSize = (size_t) st.st_size;
        char* linkName = malloc(linkSize + 1);
        if (!linkName) return -1;

        ssize_t bytes = readlink(path, linkName, linkSize + 1);
        if (bytes < 0) {
            free(linkName);
            return -1;
        }

        if ((size_t) bytes > linkSize) {
            free(linkName);
            errno = EIO;
            return -1;
        }
        linkName[bytes] = '\0';

        const char* slash;
        if (!strchr(linkName, '/')) {
            free(linkName);
            parentPath = strdup(path);
            if (!parentPath) return -1;
        } else if (linkName[0] == '/' || !(slash = strrchr(path, '/'))) {
            parentPath = linkName;
        } else {
            size_t prefixLength = slash - path + 1;
            parentPath = malloc(bytes + prefixLength + 1);
            if (!parentPath) {
                free(linkName);
                return -1;
            }
            memcpy(parentPath, path, prefixLength);
            memcpy(parentPath + prefixLength, linkName, bytes + 1);
            free(linkName);
        }
    } else {
        parentPath = strdup(path);
        if (!parentPath) return -1;
    }

    char* slash = strrchr(parentPath, '/');

    int fd;
    if (slash) {
        slash[1] = '\0';
        fd = open(parentPath, O_SEARCH | O_CLOEXEC | O_DIRECTORY);
    } else {
        fd = open(".", O_SEARCH | O_CLOEXEC | O_DIRECTORY);
    }
    free(parentPath);
    return fd;
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
        fd = openParentDir(path);
    }

    if (fd < 0) return NULL;

    char* name = malloc(1);
    if (!name) {
        close(fd);
        return NULL;
    }
    *name = '\0';
    size_t length = 0;

    while (true) {
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
