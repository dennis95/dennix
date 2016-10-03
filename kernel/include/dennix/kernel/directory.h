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

/* kernel/include/dennix/kernel/directory.h
 * Directory Vnode.
 */

#ifndef KERNEL_DIRECTORY_H
#define KERNEL_DIRECTORY_H

#include <dennix/kernel/vnode.h>

class DirectoryVnode : public Vnode {
public:
    DirectoryVnode(DirectoryVnode* parent, mode_t mode);
    void addChildNode(const char* path, Vnode* vnode);
    virtual Vnode* openat(const char* path, int flags, mode_t mode);
    virtual ssize_t readdir(unsigned long offset, void* buffer, size_t size);
public:
    size_t childCount;
private:
    Vnode** childNodes;
    const char** fileNames;
    DirectoryVnode* parent;
};

#endif
