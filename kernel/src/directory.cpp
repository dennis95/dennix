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

/* kernel/src/directory.cpp
 * Directory Vnode.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dennix/dirent.h>
#include <dennix/fcntl.h>
#include <dennix/kernel/directory.h>

DirectoryVnode::DirectoryVnode(const Reference<DirectoryVnode>& parent,
        mode_t mode, dev_t dev) : Vnode(S_IFDIR | mode, dev), parent(parent) {
    childCount = 0;
    childNodes = nullptr;
    fileNames = nullptr;
    // st_nlink must also count the . and .. entries.
    stats.st_nlink += parent ? 1 : 2;
}

DirectoryVnode::~DirectoryVnode() {
    free(childNodes);
    free(fileNames);
    stats.st_nlink -= parent ? 1 : 2;
}

int DirectoryVnode::link(const char* name, const Reference<Vnode>& vnode) {
    AutoLock lock(&mutex);
    return linkUnlocked(name, vnode);
}

int DirectoryVnode::linkUnlocked(const char* name,
        const Reference<Vnode>& vnode) {
    if (getChildNodeUnlocked(name)) {
        errno = EEXIST;
        return -1;
    }

    Reference<Vnode>* newChildNodes = (Reference<Vnode>*)
            reallocarray(childNodes, childCount + 1, sizeof(Reference<Vnode>));
    if (!newChildNodes) return -1;
    childNodes = newChildNodes;

    char** newFileNames = (char**) reallocarray(fileNames, childCount + 1,
            sizeof(const char*));
    if (!newFileNames) return -1;
    fileNames = newFileNames;

    fileNames[childCount] = strdup(name);
    if (!fileNames[childCount]) return -1;

    // We must use placement new here because the memory returned by realloc
    // is uninitialized so we cannot call operator=.
    new (&childNodes[childCount]) Reference<Vnode>(vnode);
    childCount++;

    vnode->onLink();
    if (S_ISDIR(vnode->stat().st_mode)) {
        stats.st_nlink++;
    }

    updateTimestamps(false, true, true);
    return 0;
}

Reference<Vnode> DirectoryVnode::getChildNode(const char* name) {
    AutoLock lock(&mutex);
    return getChildNodeUnlocked(name);
}

Reference<Vnode> DirectoryVnode::getChildNodeUnlocked(const char* name) {
    if (strcmp(name, ".") == 0) {
        return this;
    } else if (strcmp(name, "..") == 0) {
        return parent ? parent : this;
    }

    for (size_t i = 0; i < childCount; i++) {
        if (strcmp(name, fileNames[i]) == 0) {
            return childNodes[i];
        }
    }

    errno = ENOENT;
    return nullptr;
}

int DirectoryVnode::mkdir(const char* name, mode_t mode) {
    AutoLock lock(&mutex);

    Reference<DirectoryVnode> newDirectory = new DirectoryVnode(this, mode,
            stats.st_dev);
    if (!newDirectory) return -1;
    if (linkUnlocked(name, newDirectory) < 0) return -1;
    return 0;
}

bool DirectoryVnode::onUnlink() {
    if (childCount > 0) {
        errno = ENOTEMPTY;
        return false;
    }
    return Vnode::onUnlink();
}

ssize_t DirectoryVnode::readdir(unsigned long offset, void* buffer,
        size_t size) {
    AutoLock lock(&mutex);
    const char* name;
    struct stat vnodeStat;

    if (offset == 0) {
        name = ".";
        vnodeStat = stats;
    } else if (offset == 1) {
        name = "..";
        vnodeStat = parent ? parent->stat() : stats;
    } else if (offset - 2 < childCount) {
        name = fileNames[offset - 2];
        vnodeStat = childNodes[offset - 2]->stat();
    } else if (offset - 2  == childCount) {
        return 0;
    } else {
        return -1;
    }

    size_t structSize = sizeof(struct dirent) + strlen(name) + 1;
    if (size >= structSize) {
        struct dirent* entry = (struct dirent*) buffer;
        entry->d_dev = vnodeStat.st_dev;
        entry->d_ino = vnodeStat.st_ino;
        entry->d_reclen = size;
        strcpy(entry->d_name, name);
    }

    updateTimestamps(true, false, false);
    return structSize;
}

int DirectoryVnode::rename(Reference<Vnode>& oldDirectory, const char* oldName,
        const char* newName) {
    AutoLock lock(&mutex);

    Reference<Vnode> vnode;
    if (oldDirectory == this) {
        vnode = getChildNodeUnlocked(oldName);
    } else {
        vnode = oldDirectory->getChildNode(oldName);

        // Check whether vnode is an ancestor of the new file.
        Reference<DirectoryVnode> dir = this;
        while (dir) {
            if (dir == vnode) {
                errno = EINVAL;
                return -1;
            }
            dir = dir->parent;
        }
    }
    if (!vnode) return -1;

    Reference<Vnode> vnode2 = getChildNodeUnlocked(newName);
    if (vnode == vnode2) return 0;

    struct stat vnodeStat = vnode->stat();

    for (size_t i = 0; i < childCount; i++) {
        if (strcmp(newName, fileNames[i]) == 0) {
            struct stat childStat = childNodes[i]->stat();
            if (!S_ISDIR(vnodeStat.st_mode) && S_ISDIR(childStat.st_mode)) {
                errno = EISDIR;
                return -1;
            }
            if (S_ISDIR(vnodeStat.st_mode) && !S_ISDIR(childStat.st_mode)) {
                errno = ENOTDIR;
                return -1;
            }

            if (unlinkUnlocked(newName, AT_REMOVEDIR | AT_REMOVEFILE) < 0) {
                return -1;
            }

            continue;
        }
    }

    if (linkUnlocked(newName, vnode) < 0) return -1;
    if (oldDirectory == this) {
        unlinkUnlocked(oldName, 0);
    } else {
        oldDirectory->unlink(oldName, 0);
    }

    if (S_ISDIR(vnodeStat.st_mode)) {
        ((Reference<DirectoryVnode>) vnode)->parent = this;
    }
    return 0;
}

int DirectoryVnode::unlink(const char* name, int flags) {
    AutoLock lock(&mutex);
    return unlinkUnlocked(name, flags);
}

int DirectoryVnode::unlinkUnlocked(const char* name, int flags) {
    for (size_t i = 0; i < childCount; i++) {
        if (strcmp(name, fileNames[i]) == 0) {
            Reference<Vnode> vnode = childNodes[i];
            struct stat vnodeStat = vnode->stat();

            // The syscall routine will always set either AT_REMOVEFILE or
            // AT_REMOVEDIR. If no flags are set we remove the entry
            // unconditionally.
            if (flags) {
                if (S_ISDIR(vnodeStat.st_mode) && !(flags & AT_REMOVEDIR)) {
                    errno = EPERM;
                    return -1;
                }
                if (!S_ISDIR(vnodeStat.st_mode) && !(flags & AT_REMOVEFILE)) {
                    errno = ENOTDIR;
                    return -1;
                }

                if (!vnode->onUnlink()) return -1;
            } else {
                // DirectoryVnode::onUnlink fails if the directory is not empty.
                // Directly call the one from Vnode to prevent this failure.
                vnode->Vnode::onUnlink();
            }

            if (S_ISDIR(vnode->stat().st_mode)) {
                stats.st_nlink--;
            }

            free(fileNames[i]);
            if (i != childCount - 1) {
                childNodes[i] = childNodes[childCount - 1];
                fileNames[i] = fileNames[childCount - 1];
            }
            childNodes[--childCount].~Reference();

            // Resize the list. Reallocation failure is not an error because we
            // are just making the list smaller.
            Reference<Vnode>* newChildNodes = (Reference<Vnode>*)
                    realloc(childNodes, childCount * sizeof(Reference<Vnode>));
            char** newFileNames = (char**) realloc(fileNames, childCount *
                    sizeof(const char*));
            if (newChildNodes) {
                childNodes = newChildNodes;
            }
            if (newFileNames) {
                fileNames = newFileNames;
            }

            updateTimestamps(false, true, true);
            return 0;
        }
    }

    errno = ENOENT;
    return -1;
}
