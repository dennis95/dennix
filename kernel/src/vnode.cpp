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

/* kernel/src/vnode.cpp
 * Vnode class.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dennix/kernel/vnode.h>

static ino_t nextIno = 0;

Vnode::Vnode(mode_t mode, dev_t dev, ino_t ino) {
    this->mode = mode;
    this->dev = dev;
    this->ino = ino;

    if (!ino) {
        this->ino = nextIno++;
    }
}

Reference<Vnode> resolvePath(const Reference<Vnode>& vnode, const char* path) {
    if (!*path) {
        errno = ENOENT;
        return nullptr;
    }

    Reference<Vnode> currentVnode = vnode;
    char* pathCopy = strdup(path);
    if (!pathCopy) {
        errno = ENFILE;
        return nullptr;
    }

    char* currentName = pathCopy;
    char* slash = strchr(currentName, '/');

    while (slash) {
        *slash = '\0';
        if (*currentName) {
            currentVnode = currentVnode->getChildNode(currentName);
            if (!currentVnode) {
                free(pathCopy);
                return nullptr;
            }

            if (!S_ISDIR(currentVnode->mode)) {
                free(pathCopy);
                errno = ENOTDIR;
                return nullptr;
            }
        }
        currentName = slash + 1;
        slash = strchr(currentName, '/');
    }

    if (*currentName) {
        currentVnode = currentVnode->getChildNode(currentName);
    }

    free(pathCopy);
    return currentVnode;
}

// Default implementation. Inheriting classes will override these functions.
int Vnode::ftruncate(off_t /*length*/) {
    errno = EBADF;
    return -1;
}

Reference<Vnode> Vnode::getChildNode(const char* /*path*/) {
    errno = EBADF;
    return nullptr;
}

bool Vnode::isSeekable() {
    return false;
}

int Vnode::mkdir(const char* /*name*/, mode_t /*mode*/) {
    errno = ENOTDIR;
    return -1;
}

bool Vnode::onUnlink() {
    return true;
}

ssize_t Vnode::pread(void* /*buffer*/, size_t /*size*/, off_t /*offset*/) {
    errno = EBADF;
    return -1;
}

ssize_t Vnode::pwrite(const void* /*buffer*/, size_t /*size*/,
        off_t /*offset*/) {
    errno = EBADF;
    return -1;
}

ssize_t Vnode::read(void* /*buffer*/, size_t /*size*/) {
    errno = EBADF;
    return -1;
}

ssize_t Vnode::readdir(unsigned long /*offset*/, void* /*buffer*/,
        size_t /*size*/) {
    errno = EBADF;
    return -1;
}

int Vnode::rename(Reference<Vnode>& /*oldDirectory*/, const char* /*oldName*/,
        const char* /*newName*/) {
    errno = EBADF;
    return -1;
}

int Vnode::stat(struct stat* result) {
    result->st_dev = dev;
    result->st_ino = ino;
    result->st_mode = mode;
    return 0;
}

int Vnode::tcgetattr(struct termios* /*result*/) {
    errno = ENOTTY;
    return -1;
}

int Vnode::tcsetattr(int /*flags*/, const struct termios* /*termio*/) {
    errno = ENOTTY;
    return -1;
}

int Vnode::unlink(const char* /*name*/, int /*flags*/) {
    errno = ENOTDIR;
    return -1;
}

ssize_t Vnode::write(const void* /*buffer*/, size_t /*size*/) {
    errno = EBADF;
    return -1;
}
