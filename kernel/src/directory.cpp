/* Copyright (c) 2016, Dennis Wölfing
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
#include <string.h>
#include <dennix/dirent.h>
#include <dennix/stat.h>
#include <dennix/kernel/directory.h>

DirectoryVnode::DirectoryVnode(DirectoryVnode* parent, mode_t mode)
        : Vnode(S_IFDIR | mode) {
    childCount = 0;
    childNodes = nullptr;
    fileNames = nullptr;
    this->parent = parent;
}

void DirectoryVnode::addChildNode(const char* path, Vnode* vnode) {
    Vnode** newChildNodes = new Vnode*[childCount + 1];
    const char** newFileNames = new const char*[childCount + 1];

    memcpy(newChildNodes, childNodes, sizeof(Vnode*) * childCount);
    memcpy(newFileNames, fileNames, sizeof(const char*) * childCount);

    childNodes = newChildNodes;
    fileNames = newFileNames;

    childNodes[childCount] = vnode;
    fileNames[childCount] = strdup(path);
    childCount++;
}

Vnode* DirectoryVnode::openat(const char* path, int flags, mode_t mode) {
    // TODO: We currently ignore flags.
    while (*path == '/') {
        path++;
    }

    size_t length = strcspn(path, "/");

    if (length == 0) {
        return this;
    }

    if (strncmp(path, ".", length) == 0) {
        return openat(path + length, flags, mode);
    } else if (length == 2 && strncmp(path, "..", length) == 0) {
        Vnode* dotdot = parent ? parent : this;
        return dotdot->openat(path + length, flags, mode);
    }

    for (size_t i = 0; i < childCount; i++) {
        if (strncmp(path, fileNames[i], length) == 0) {
            if (path[length] == '\0') {
                return childNodes[i];
            } else {
                return childNodes[i]->openat(path + length, flags, mode);
            }
        }
    }

    errno = ENOENT;
    return nullptr;
}

ssize_t DirectoryVnode::readdir(unsigned long offset, void* buffer,
        size_t size) {
    const char* name;
    if (offset == 0) {
        name = ".";
    } else if (offset == 1) {
        name = "..";
    } else if (offset - 2 < childCount) {
        name = fileNames[offset - 2];
    } else if (offset - 2  == childCount) {
        return 0;
    } else {
        return -1;
    }

    size_t structSize = sizeof(struct dirent) + strlen(name) + 1;
    if (size >= structSize) {
        struct dirent* entry = (struct dirent*) buffer;
        entry->d_reclen = size;
        strcpy(entry->d_name, name);
    }

    return structSize;
}
