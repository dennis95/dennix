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

/* kernel/src/directory.cpp
 * Directory Vnode.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dennix/dirent.h>
#include <dennix/stat.h>
#include <dennix/kernel/directory.h>

DirectoryVnode::DirectoryVnode(DirectoryVnode* parent, mode_t mode, dev_t dev,
        ino_t ino) : Vnode(S_IFDIR | mode, dev, ino) {
    childCount = 0;
    childNodes = nullptr;
    fileNames = nullptr;
    mutex = KTHREAD_MUTEX_INITIALIZER;
    this->parent = parent;
}

DirectoryVnode::~DirectoryVnode() {
    free(childNodes);
    free(fileNames);
}

bool DirectoryVnode::addChildNode(const char* path, Vnode* vnode) {
    AutoLock lock(&mutex);

    Vnode** newChildNodes = (Vnode**) reallocarray(childNodes, childCount + 1,
            sizeof(Vnode*));
    if (!newChildNodes) return false;
    const char** newFileNames = (const char**) reallocarray(fileNames,
            childCount + 1, sizeof(const char*));
    if (!newFileNames) return false;

    childNodes = newChildNodes;
    fileNames = newFileNames;

    childNodes[childCount] = vnode;
    fileNames[childCount] = strdup(path);
    childCount++;
    return true;
}

Vnode* DirectoryVnode::getChildNode(const char* path) {
    AutoLock lock(&mutex);

    if (strcmp(path, ".") == 0) {
        return this;
    } else if (strcmp(path, "..") == 0) {
        return parent ? parent : this;
    }

    for (size_t i = 0; i < childCount; i++) {
        if (strcmp(path, fileNames[i]) == 0) {
            return childNodes[i];
        }
    }

    errno = ENOENT;
    return nullptr;
}

ssize_t DirectoryVnode::readdir(unsigned long offset, void* buffer,
        size_t size) {
    AutoLock lock(&mutex);
    const char* name;
    Vnode* vnode;

    if (offset == 0) {
        name = ".";
        vnode = this;
    } else if (offset == 1) {
        name = "..";
        vnode = parent ? parent : this;
    } else if (offset - 2 < childCount) {
        name = fileNames[offset - 2];
        vnode = childNodes[offset - 2];
    } else if (offset - 2  == childCount) {
        return 0;
    } else {
        return -1;
    }

    size_t structSize = sizeof(struct dirent) + strlen(name) + 1;
    if (size >= structSize) {
        struct dirent* entry = (struct dirent*) buffer;
        entry->d_dev = vnode->dev;
        entry->d_ino = vnode->ino;
        entry->d_reclen = size;
        strcpy(entry->d_name, name);
    }

    return structSize;
}
