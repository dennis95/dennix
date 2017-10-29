/* Copyright (c) 2016, 2017 Dennis Wölfing
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

/* kernel/src/filedescription.cpp
 * FileDescription class.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dennix/fcntl.h>
#include <dennix/kernel/directory.h>
#include <dennix/kernel/file.h>
#include <dennix/kernel/filedescription.h>

FileDescription::FileDescription(const Reference<Vnode>& vnode)
        : vnode(vnode) {
    offset = 0;
}

FileDescription* FileDescription::openat(const char* path, int flags,
        mode_t mode) {
    Reference<Vnode> node = resolvePath(vnode, path);
    if (!node) {
        if (!(flags & O_CREAT)) return nullptr;
        char* pathCopy = strdup(path);
        char* slash = strrchr(pathCopy, '/');
        char* newFileName;

        if (!slash || slash == pathCopy) {
            node = vnode;
            newFileName = slash ? pathCopy + 1 : pathCopy;
        } else {
            *slash = '\0';
            newFileName = slash + 1;
            node = resolvePath(vnode, pathCopy);
            if (!node) {
                free(pathCopy);
                return nullptr;
            }
        }

        if (!S_ISDIR(node->mode)) {
            free(pathCopy);
            errno = ENOTDIR;
            return nullptr;
        }

        Reference<FileVnode> file(new FileVnode(nullptr, 0, mode & 07777,
                vnode->dev, 0));
        if (node->link(newFileName, file) < 0) {
            free(pathCopy);
            return nullptr;
        }

        free(pathCopy);
        node = file;
    }

    if (flags & O_TRUNC) {
        node->ftruncate(0);
    }

    return new FileDescription(node);
}

ssize_t FileDescription::read(void* buffer, size_t size) {
    if (vnode->isSeekable()) {
        ssize_t result = vnode->pread(buffer, size, offset);

        if (result != -1) {
            offset += result;
        }
        return result;
    }
    return vnode->read(buffer, size);
}

ssize_t FileDescription::readdir(unsigned long offset, void* buffer,
        size_t size) {
    return vnode->readdir(offset, buffer, size);
}

int FileDescription::tcgetattr(struct termios* result) {
    return vnode->tcgetattr(result);
}

int FileDescription::tcsetattr(int flags, const struct termios* termio) {
    return vnode->tcsetattr(flags, termio);
}

ssize_t FileDescription::write(const void* buffer, size_t size) {
    if (vnode->isSeekable()) {
        ssize_t result = vnode->pwrite(buffer, size, offset);

        if (result != -1) {
            offset += result;
        }
        return result;
    }
    return vnode->write(buffer, size);
}
