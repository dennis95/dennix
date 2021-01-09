/* Copyright (c) 2016, 2017, 2018, 2020, 2021 Dennis Wölfing
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
    DirectoryVnode(const Reference<DirectoryVnode>& parent, mode_t mode,
            dev_t dev);
    ~DirectoryVnode();
    Reference<Vnode> getChildNode(const char* name) override;
    Reference<Vnode> getChildNode(const char* path, size_t length) override;
    size_t getDirectoryEntries(void** buffer) override;
    int link(const char* name, const Reference<Vnode>& vnode) override;
    off_t lseek(off_t offset, int whence) override;
    int mkdir(const char* name, mode_t mode) override;
    bool onUnlink() override;
    Reference<Vnode> open(const char* name, int flags, mode_t mode) override;
    int rename(const Reference<Vnode>& oldDirectory, const char* oldName,
            const char* newName) override;
    int unlink(const char* path, int flags) override;
private:
    Reference<Vnode> getChildNodeUnlocked(const char* name, size_t length);
    int linkUnlocked(const char* name, size_t length,
            const Reference<Vnode>& vnode);
    int unlinkUnlocked(const char* path, int flags);
public:
    size_t childCount;
private:
    Reference<Vnode>* childNodes;
    char** fileNames;
protected:
    Reference<DirectoryVnode> parent;
};

#endif
