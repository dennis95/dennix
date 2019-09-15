/* Copyright (c) 2016, 2017, 2018, 2019 Dennis Wölfing
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
#include <dennix/seek.h>
#include <dennix/kernel/directory.h>
#include <dennix/kernel/file.h>
#include <dennix/kernel/filedescription.h>

#define FILE_STATUS_FLAGS (O_APPEND | O_NONBLOCK | O_SYNC)

FileDescription::FileDescription(const Reference<Vnode>& vnode, int flags)
        : vnode(vnode) {
    offset = 0;
    this->flags = flags & (O_ACCMODE | FILE_STATUS_FLAGS);
}

int FileDescription::fcntl(int cmd, int param) {
    switch (cmd) {
    case F_GETFL:
        return flags;
    case F_SETFL:
        flags = (param & FILE_STATUS_FLAGS) | (flags & O_ACCMODE);
        return 0;
    default:
        errno = EINVAL;
        return -1;
    }
}

off_t FileDescription::lseek(off_t offset, int whence) {
    if (whence == SEEK_CUR) {
        if (__builtin_add_overflow(offset, this->offset, &offset)) {
            errno = EOVERFLOW;
            return -1;
        }
    }

    off_t result = vnode->lseek(offset, whence);

    if (result < 0) return -1;

    this->offset = result;
    return result;
}

Reference<FileDescription> FileDescription::openat(const char* path, int flags,
        mode_t mode) {
    Reference<Vnode> node = resolvePath(vnode, path);
    if (!node) {
        if (!(flags & O_CREAT)) return nullptr;
        char* pathCopy = strdup(path);
        if (!pathCopy) return nullptr;

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

        if (!S_ISDIR(node->stat().st_mode)) {
            free(pathCopy);
            errno = ENOTDIR;
            return nullptr;
        }

        Reference<FileVnode> file = new FileVnode(nullptr, 0, mode & 07777,
                vnode->stat().st_dev);
        if (!file || node->link(newFileName, file) < 0) {
            free(pathCopy);
            return nullptr;
        }

        free(pathCopy);
        node = file;
    }

    if (flags & O_TRUNC) {
        node->ftruncate(0);
    }

    return new FileDescription(node, flags);
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
        bool append = flags & O_APPEND;
        ssize_t result = vnode->pwrite(buffer, size, append ? -1 : offset);
        if (result != -1) {
            offset = append ? vnode->stat().st_size : offset + result;
        }
        return result;
    }
    return vnode->write(buffer, size);
}
