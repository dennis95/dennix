/* Copyright (c) 2021 Dennis Wölfing
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

/* kernel/src/ext234vnode.cpp
 * ext2/ext3/ext4 Vnodes.
 */

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <dennix/fcntl.h>
#include <dennix/poll.h>
#include <dennix/seek.h>
#include <dennix/kernel/ext234fs.h>

static unsigned char typeToDT(uint8_t type) {
    return type == 1 ? DT_REG :
            type == 2 ? DT_DIR :
            type == 3 ? DT_CHR :
            type == 4 ? DT_BLK :
            type == 5 ? DT_FIFO :
            type == 6 ? DT_SOCK :
            type == 7 ? DT_LNK : DT_UNKNOWN;
}

static uint8_t dtToType(unsigned char dt) {
    return dt == DT_REG ? 1 :
            dt == DT_DIR ? 2 :
            dt == DT_CHR ? 3 :
            dt == DT_BLK ? 4 :
            dt == DT_FIFO ? 5 :
            dt == DT_SOCK ? 6 :
            dt == DT_LNK ? 7 : 0;
}

Ext234Vnode::Ext234Vnode(Ext234Fs* fs, ino_t ino, const Inode* inode,
        uint64_t inodeAddress) : Vnode(inode->i_mode, fs->dev), inode(*inode),
        inodeAddress(inodeAddress) {
    filesystem = fs;
    inodeModified = false;
    mounted = nullptr;

    stats.st_ino = ino;
    stats.st_nlink = inode->i_links_count;
    stats.st_uid = inode->i_uid;
    stats.st_gid = inode->i_gid;
    stats.st_size = fs->getInodeSize(inode);
    stats.st_atim = fs->getInodeATime(inode);
    stats.st_ctim = fs->getInodeCTime(inode);
    stats.st_mtim = fs->getInodeMTime(inode);
    stats.st_blksize = filesystem->blockSize;
    stats.st_blocks = inode->i_blocks;
}

Ext234Vnode::~Ext234Vnode() {
    if (S_ISDIR(stats.st_mode) && stats.st_nlink == 1) {
        // Decrease count for the . entry.
        stats.st_nlink = 0;
        inode.i_links_count = 0;
    }
    if (stats.st_nlink == 0) {
        // File has been deleted
        struct timespec now;
        Clock::get(CLOCK_REALTIME)->getTime(&now);
        filesystem->setTime(&now, &inode.i_dtime, nullptr);
        filesystem->resizeInode(stats.st_ino, &inode, 0);
        inodeModified = true;
    }

    if (inodeModified) {
        filesystem->writeInode(&inode, inodeAddress);
    }

    if (stats.st_nlink == 0) {
        filesystem->deallocateInode(stats.st_ino, S_ISDIR(stats.st_mode));
    }
    stats.st_nlink = 0;
}

bool Ext234Vnode::addChildNode(const char* name, size_t nameLength, ino_t ino,
        unsigned char dt) {
    size_t neededSize = ALIGNUP(sizeof(DirectoryEntry) + nameLength, 4);

    uint64_t blockNum = 0;
    while (blockNum * filesystem->blockSize < (uint64_t) stats.st_size) {
        char* block = new char[filesystem->blockSize];
        if (!block) return false;
        if (!filesystem->readInodeData(&inode, blockNum * filesystem->blockSize,
                block, filesystem->blockSize)) {
            delete[] block;
            return false;
        }

        size_t offset = 0;
        while (offset < filesystem->blockSize) {
            DirectoryEntry* entry = (DirectoryEntry*) (block + offset);

            if (entry->rec_len < 8) {
                delete[] block;
                errno = EIO;
                return false;
            }

            if (entry->inode != 0) {
                size_t length = ALIGNUP(sizeof(DirectoryEntry) +
                        entry->name_len, 4);
                if (entry->rec_len >= length + neededSize) {
                    size_t remainingLength = entry->rec_len - length;
                    entry->rec_len = length;
                    entry = (DirectoryEntry*) (block + offset + length);
                    entry->inode = 0;
                    entry->rec_len = remainingLength;
                }
            }

            if (entry->inode == 0 && entry->rec_len >= neededSize) {
                entry->inode = ino;
                entry->name_len = nameLength;
                if (filesystem->hasIncompatFeature(INCOMPAT_FILETYPE)) {
                    entry->file_type = dtToType(dt);
                } else {
                    entry->file_type = 0;
                }
                memcpy(entry->name, name, nameLength);

                bool result = filesystem->writeInodeData(&inode,
                        blockNum * filesystem->blockSize, block,
                        filesystem->blockSize);
                delete[] block;
                return result;
            }

            offset += entry->rec_len;
        }

        delete[] block;
        blockNum++;
    }

    // No free space for the new entry was found.
    if (!filesystem->resizeInode(stats.st_ino, &inode,
            stats.st_size + filesystem->blockSize)) {
        return false;
    }
    stats.st_size += filesystem->blockSize;

    char* block = new char[filesystem->blockSize];
    if (!block) return false;
    memset(block, 0, filesystem->blockSize);
    DirectoryEntry* entry = (DirectoryEntry*) block;
    entry->inode = ino;
    entry->rec_len = neededSize;
    entry->name_len = nameLength;
    if (filesystem->hasIncompatFeature(INCOMPAT_FILETYPE)) {
        entry->file_type = dtToType(dt);
    } else {
        entry->file_type = 0;
    }
    memcpy(entry->name, name, nameLength);

    entry = (DirectoryEntry*) &block[neededSize];
    entry->inode = 0;
    entry->rec_len = filesystem->blockSize - neededSize;

    if (!filesystem->writeInodeData(&inode, blockNum * filesystem->blockSize,
            block, filesystem->blockSize)) {
        delete[] block;
        return false;
    }
    delete[] block;
    return true;
}

int Ext234Vnode::chmod(mode_t mode) {
    AutoLock lock(&mutex);
    if (filesystem->readonly) {
        errno = EROFS;
        return -1;
    }

    stats.st_mode = (stats.st_mode & ~07777) | (mode & 07777);
    inode.i_mode = stats.st_mode;
    updateTimestamps(false, true, false);
    return 0;
}

int Ext234Vnode::chown(uid_t uid, gid_t gid) {
    AutoLock lock(&mutex);
    if (filesystem->readonly) {
        errno = EROFS;
        return -1;
    }

    if (uid != (uid_t) -1) {
        stats.st_uid = uid;
    }
    if (gid != (gid_t) -1) {
        stats.st_gid = gid;
    }
    if (stats.st_mode & 0111) {
        stats.st_mode &= ~(S_ISUID | S_ISGID);
    }
    inode.i_uid = stats.st_uid;
    inode.i_gid = stats.st_gid;
    inode.i_mode = stats.st_mode;
    updateTimestamps(false, true, false);
    return 0;
}

uint64_t Ext234Vnode::findDirectoryEntry(const char* name, size_t nameLength,
        DirectoryEntry* de) {
    off_t bytesRead = 0;
    uint64_t blockNum = 0;
    char* block = new char[filesystem->blockSize];
    if (!block) return -1;

    while (bytesRead < stats.st_size) {
        if (!filesystem->readInodeData(&inode, blockNum * filesystem->blockSize,
                block, filesystem->blockSize)) {
            delete[] block;
            return -1;
        }

        size_t offset = 0;
        while (offset < filesystem->blockSize) {
            DirectoryEntry* entry = (DirectoryEntry*) (block + offset);

            if (entry->rec_len < 8) {
                delete[] block;
                errno = EIO;
                return -1;
            }

            if (entry->inode == 0) {
                offset += entry->rec_len;
                continue;
            }

            if (entry->name_len == nameLength && memcmp(name, entry->name,
                    nameLength) == 0) {
                *de = *entry;
                delete[] block;
                return blockNum * filesystem->blockSize + offset;
            }

            offset += entry->rec_len;
        }

        bytesRead += filesystem->blockSize;
        blockNum++;
    }

    delete[] block;
    return -1;
}

int Ext234Vnode::ftruncate(off_t length) {
    AutoLock lock(&mutex);
    if (filesystem->readonly) {
        errno = EROFS;
        return -1;
    }

    if (!S_ISREG(stats.st_mode) || length < 0) {
        errno = EINVAL;
        return -1;
    }

    off_t oldSize = stats.st_size;
    if (!filesystem->resizeInode(stats.st_ino, &inode, length)) {
        return -1;
    }
    stats.st_size = length;

    while (length > oldSize) {
        char* buffer = new char[filesystem->blockSize];
        if (!buffer) return -1;
        memset(buffer, 0, filesystem->blockSize);
        size_t diff = length - oldSize;
        if (diff > filesystem->blockSize) diff = filesystem->blockSize;
        if (!filesystem->writeInodeData(&inode, oldSize, buffer, diff)) {
            delete[] buffer;
            return -1;
        }
        delete[] buffer;
        oldSize += diff;
    }

    updateTimestamps(false, true, true);
    return 0;
}

Reference<Vnode> Ext234Vnode::getChildNode(const char* name) {
    return getChildNode(name, strlen(name));
}

Reference<Vnode> Ext234Vnode::getChildNode(const char* path, size_t length) {
    AutoLock lock(&mutex);
    return getChildNodeUnlocked(path, length);
}

Reference<Vnode> Ext234Vnode::getChildNodeUnlocked(const char* path,
        size_t length) {
    if (!S_ISDIR(stats.st_mode)) {
        errno = ENOTDIR;
        return nullptr;
    }

    if (stats.st_ino == 2 && length == 2 && strncmp(path, "..", 2) == 0) {
        return filesystem->mountPoint->getChildNode(path, length);
    }

    DirectoryEntry entry;
    if (findDirectoryEntry(path, length, &entry) != (uint64_t) -1) {
        return filesystem->getVnode(entry.inode);
    }

    errno = ENOENT;
    return nullptr;
}

size_t Ext234Vnode::getDirectoryEntries(void** buffer, int flags) {
    AutoLock lock(&mutex);

    if (!S_ISDIR(stats.st_mode)) {
        *buffer = nullptr;
        errno = ENOTDIR;
        return 0;
    }

    // We do not know in advance how large the buffer needs to be, but the size
    // on disk is a good estimate.
    *buffer = malloc(stats.st_size);
    if (!*buffer) return 0;
    size_t allocatedSize = stats.st_size;

    size_t bufferOffset = 0;
    off_t bytesRead = 0;
    size_t blockNum = 0;
    while (bytesRead < stats.st_size) {
        char* block = new char[filesystem->blockSize];
        if (!block) {
            free(*buffer);
            *buffer = nullptr;
            return 0;
        }
        if (!filesystem->readInodeData(&inode, blockNum * filesystem->blockSize,
                block, filesystem->blockSize)) {
            delete[] block;
            free(*buffer);
            *buffer = nullptr;
            return 0;
        }

        size_t offset = 0;
        while (offset < filesystem->blockSize) {
            DirectoryEntry* entry = (DirectoryEntry*) (block + offset);

            if (entry->rec_len < 8) {
                delete[] block;
                free(*buffer);
                *buffer = nullptr;
                errno = EIO;
                return 0;
            }

            if (entry->inode == 0) {
                offset += entry->rec_len;
                continue;
            }

            reclen_t reclen = ALIGNUP(sizeof(posix_dent) + entry->name_len + 1,
                    alignof(posix_dent));

            if (bufferOffset + reclen > allocatedSize) {
                allocatedSize = allocatedSize + ALIGNUP(reclen, 1024);
                void* newBuffer = realloc(*buffer, allocatedSize);
                if (!newBuffer) {
                    delete[] block;
                    free(*buffer);
                    *buffer = nullptr;
                    return 0;
                }
                *buffer = newBuffer;
            }

            posix_dent* dent = (posix_dent*) ((char*) *buffer + bufferOffset);
            dent->d_ino = entry->inode;

            // If another filesystem has been mounted at a directory we must
            // give the inode number for that filesystem.
            if (dent->d_ino != stats.st_ino) {
                Reference<Ext234Vnode> vnode =
                        filesystem->getVnodeIfOpen(entry->inode);
                if (vnode && vnode->mounted) {
                    Reference<Vnode> vnode2 = vnode->resolve();
                    dent->d_ino = vnode2->stat().st_ino;
                }
            }

            dent->d_reclen = reclen;

            if (filesystem->hasIncompatFeature(INCOMPAT_FILETYPE)) {
                dent->d_type = typeToDT(entry->file_type);
            } else if (flags & DT_FORCE_TYPE) {
                Reference<Vnode> vnode = filesystem->getVnode(entry->inode);
                if (vnode) {
                    dent->d_type = IFTODT(vnode->stat().st_mode);
                } else {
                    dent->d_type = DT_UNKNOWN;
                }
            } else {
                dent->d_type = DT_UNKNOWN;
            }

            memcpy(&dent->d_name, entry->name, entry->name_len);
            dent->d_name[entry->name_len] = '\0';

            bufferOffset += dent->d_reclen;
            offset += entry->rec_len;
        }

        delete[] block;
        bytesRead += filesystem->blockSize;
        blockNum++;
    }

    void* result = realloc(*buffer, bufferOffset);
    if (result) {
        *buffer = result;
    }

    return bufferOffset;
}

char* Ext234Vnode::getLinkTarget() {
    AutoLock lock(&mutex);
    assert(S_ISLNK(stats.st_mode));

    if (stats.st_size < 60) {
        return strndup((char*) inode.i_block, stats.st_size);
    } else {
        char* result = (char*) malloc(stats.st_size);
        if (!result) return nullptr;
        if (!filesystem->readInodeData(&inode, 0, result, stats.st_size)) {
            free(result);
            return nullptr;
        }
        return result;
    }
}

bool Ext234Vnode::isSeekable() {
    return S_ISREG(stats.st_mode);
}

int Ext234Vnode::link(const char* name, const Reference<Vnode>& vnode) {
    AutoLock lock(&mutex);
    if (filesystem->readonly) {
        errno = EROFS;
        return -1;
    }

    return linkUnlocked(name, strlen(name), vnode);
}

int Ext234Vnode::linkUnlocked(const char* name, size_t nameLength,
        const Reference<Vnode>& vnode) {
    if (!S_ISDIR(stats.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }

    if (stats.st_nlink == 0) {
        // The directory has been deleted.
        errno = ENOENT;
        return -1;
    }

    if (vnode->stat().st_dev != stats.st_dev) {
        errno = EXDEV;
        return -1;
    }

    if (nameLength > 255) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (getChildNodeUnlocked(name, nameLength)) {
        errno = EEXIST;
        return -1;
    }

    struct stat st = vnode->stat();
    if (!addChildNode(name, nameLength, st.st_ino, IFTODT(st.st_mode))) {
        return -1;
    }
    updateTimestamps(false, true, true);
    vnode->onLink();
    return 0;
}

off_t Ext234Vnode::lseek(off_t offset, int whence) {
    AutoLock lock(&mutex);
    off_t base;

    if (whence == SEEK_SET || whence == SEEK_CUR) {
        base = 0;
    } else if (whence == SEEK_END) {
        if (S_ISDIR(stats.st_mode)) {
            // We have no easy way to seek to the end without iterating through
            // the whole directory. However since behavior of posix_getdents is
            // unspecified after seeking to a value that was not previously
            // returned we can just seek somewhere past the end instead.
        }
        base = stats.st_size;
    } else {
        errno = EINVAL;
        return -1;
    }

    off_t result;
    if (__builtin_add_overflow(base, offset, &result) || result < 0) {
        errno = EINVAL;
        return -1;
    }

    return result;
}

int Ext234Vnode::mkdir(const char* name, mode_t mode) {
    AutoLock lock(&mutex);
    if (filesystem->readonly) {
        errno = EROFS;
        return -1;
    }

    uint64_t blockGroup = filesystem->getBlockGroup(stats.st_ino);
    ino_t ino = filesystem->createInode(blockGroup, (mode & 07777) | S_IFDIR);
    if (ino == 0) return -1;
    Reference<Ext234Vnode> vnode = filesystem->getVnode(ino);
    if (!vnode) return -1;
    vnode->addChildNode(".", 1, ino, DT_DIR);
    vnode->addChildNode("..", 2, stats.st_ino, DT_DIR);
    vnode->updateTimestampsLocked(true, true, true);
    vnode->stats.st_nlink = 1;
    stats.st_nlink++;
    inode.i_links_count = stats.st_nlink;

    if (linkUnlocked(name, strcspn(name, "/"), vnode) < 0) {
        return -1;
    }
    return 0;
}

int Ext234Vnode::mount(FileSystem* filesystem) {
    AutoLock lock(&mutex);

    if (!S_ISDIR(stats.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }

    if (mounted || stats.st_ino == 2) {
        errno = EBUSY;
        return -1;
    }

    mounted = filesystem;
    return 0;
}

void Ext234Vnode::onLink() {
    AutoLock lock(&mutex);
    updateTimestamps(false, true, false);
    stats.st_nlink++;
    inode.i_links_count = stats.st_nlink;
}

bool Ext234Vnode::onUnlink(bool force) {
    AutoLock lock(&mutex);

    if (!force && mounted) {
        errno = EBUSY;
        return false;
    }

    if (!force && S_ISDIR(stats.st_mode)) {
        size_t count = 0;
        uint64_t blockNum = 0;
        while (count < 3 &&
                blockNum * filesystem->blockSize < (uint64_t) stats.st_size) {
            char* block = new char[filesystem->blockSize];
            if (!block) return false;
            if (!filesystem->readInodeData(&inode, blockNum *
                    filesystem->blockSize, block, filesystem->blockSize)) {
                delete[] block;
                return false;
            }

            size_t offset = 0;
            while (count < 3 && offset < filesystem->blockSize) {
                DirectoryEntry* entry = (DirectoryEntry*) (block + offset);

                if (entry->rec_len < 8) {
                    delete[] block;
                    errno = EIO;
                    return false;
                }

                if (entry->inode != 0) {
                    count++;
                }

                offset += entry->rec_len;
            }

            delete[] block;
            blockNum++;
        }

        if (count >= 3) return false;
        filesystem->resizeInode(stats.st_ino, &inode, 0);
        stats.st_nlink--;
    }

    updateTimestamps(false, true, false);
    stats.st_nlink--;
    inode.i_links_count = stats.st_nlink;
    return true;
}

Reference<Vnode> Ext234Vnode::open(const char* name, int flags, mode_t mode) {
    AutoLock lock(&mutex);

    if (!S_ISDIR(stats.st_mode)) {
        errno = ENOTDIR;
        return nullptr;
    }

    size_t length = strcspn(name, "/");
    Reference<Vnode> vnode = getChildNodeUnlocked(name, length);
    if (!vnode) {
        if (!(flags & O_CREAT)) return nullptr;
        if (filesystem->readonly) {
            errno = EROFS;
            return nullptr;
        }

        uint64_t blockGroup = filesystem->getBlockGroup(stats.st_ino);
        ino_t ino = filesystem->createInode(blockGroup,
                (mode & 07777) | S_IFREG);
        if (ino == 0) return nullptr;
        vnode = filesystem->getVnode(ino);

        if (!vnode || linkUnlocked(name, length, vnode) < 0) {
            return nullptr;
        }
        vnode->updateTimestampsLocked(true, true, true);
    } else {
        if (flags & O_EXCL) {
            errno = EEXIST;
            return nullptr;
        } else if (flags & O_NOCLOBBER && S_ISREG(vnode->stats.st_mode)) {
            errno = EEXIST;
            return nullptr;
        }
    }

    return vnode;
}

short Ext234Vnode::poll() {
    if (S_ISREG(stats.st_mode)) {
        return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
    }
    return 0;
}

ssize_t Ext234Vnode::pread(void* buffer, size_t size, off_t offset,
        int /*flags*/) {
    AutoLock lock(&mutex);

    if (S_ISDIR(stats.st_mode)) {
        errno = EISDIR;
        return -1;
    } else if (!S_ISREG(stats.st_mode)) {
        errno = EIO;
        return -1;
    }

    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    if (offset > stats.st_size) return 0;

    if ((off_t) size > stats.st_size ||
            stats.st_size - (off_t) size < offset) {
        size = stats.st_size - offset;
    }

    if (!filesystem->readInodeData(&inode, offset, buffer, size)) {
        return -1;
    }

    updateTimestamps(true, false, false);
    return size;
}

ssize_t Ext234Vnode::pwrite(const void* buffer, size_t size, off_t offset,
        int flags) {
    AutoLock lock(&mutex);
    if (filesystem->readonly) {
        errno = EROFS;
        return -1;
    }

    if (S_ISDIR(stats.st_mode)) {
        errno = EISDIR;
        return -1;
    } else if (!S_ISREG(stats.st_mode)) {
        errno = EIO;
        return -1;
    }

    if (size == 0) return 0;

    if (flags & O_APPEND) {
        offset = stats.st_size;
    }
    assert(offset >= 0);

    off_t newSize;
    if (__builtin_add_overflow(offset, size, &newSize)) {
        errno = ENOSPC;
        return -1;
    }

    if (newSize > stats.st_size) {
        if (!filesystem->resizeInode(stats.st_ino, &inode, newSize)) {
            return -1;
        }
        stats.st_size = newSize;
    }

    if (!filesystem->writeInodeData(&inode, offset, buffer, size)) return -1;

    updateTimestamps(false, true, true);
    return size;
}

ssize_t Ext234Vnode::readlink(char* buffer, size_t size) {
    if (!S_ISLNK(stats.st_mode)) {
        errno = EINVAL;
        return -1;
    }

    if ((off_t) size > stats.st_size) {
        size = stats.st_size;
    }

    if (stats.st_size < 60) {
        memcpy(buffer, inode.i_block, size);
        buffer[size] = '\0';
    } else if (!filesystem->readInodeData(&inode, 0, buffer, size)) {
        return -1;
    }

    updateTimestamps(true, false, false);
    return size;
}

void Ext234Vnode::removeReference() const {
    Ext234Fs* fs = filesystem;
    fs->dropVnodeReference(stats.st_ino);
    ReferenceCounted::removeReference();
    // We need release the fs mutex after the reference was dropped.
    fs->finishDropVnodeReference();
}

int Ext234Vnode::rename(const Reference<Vnode>& oldDirectory,
        const char* oldName, const char* newName) {
    AutoLock lock(&mutex);
    if (filesystem->readonly) {
        errno = EROFS;
        return -1;
    }

    if (!S_ISDIR(stats.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }

    Reference<Vnode> vnode;
    if (oldDirectory == this) {
        vnode = getChildNodeUnlocked(oldName, strcspn(oldName, "/"));
    } else {
        vnode = oldDirectory->getChildNode(oldName, strcspn(oldName, "/"));

        // Check whether vnode is an ancestor of the new file.
        Reference<Vnode> dir = getChildNodeUnlocked("..", 2);
        while (dir && dir->stat().st_dev == stats.st_dev) {
            if (dir == vnode) {
                errno = EINVAL;
                return -1;
            }
            dir = dir->getChildNode("..");
        }
    }
    if (!vnode) return -1;
    struct stat vnodeStat = vnode->stat();
    if (vnodeStat.st_dev != stats.st_dev) {
        errno = EXDEV;
        return -1;
    }

    size_t newNameLength = strcspn(newName, "/");
    DirectoryEntry entry;
    uint64_t offset = findDirectoryEntry(newName, newNameLength, &entry);
    if (offset != (uint64_t) -1) {
        if (entry.inode == vnodeStat.st_ino) return 0;

        mode_t mode;
        if (filesystem->hasIncompatFeature(INCOMPAT_FILETYPE)) {
            mode = DTTOIF(typeToDT(entry.file_type));
        } else {
            Reference<Vnode> vnode2 = filesystem->getVnode(entry.inode);
            if (!vnode2) return -1;
            mode = vnode2->stat().st_mode;
        }

        if (!S_ISDIR(vnodeStat.st_mode) && S_ISDIR(mode)) {
            errno = EISDIR;
            return -1;
        }
        if (S_ISDIR(vnodeStat.st_mode) && !S_ISDIR(mode)) {
            errno = ENOTDIR;
            return -1;
        }

        if (unlinkUnlocked(newName, AT_REMOVEDIR | AT_REMOVEFILE) < 0) {
            return -1;
        }
    }

    if (linkUnlocked(newName, newNameLength, vnode) < 0) return -1;
    if (oldDirectory == this) {
        unlinkUnlocked(oldName, 0);
    } else {
        oldDirectory->unlink(oldName, 0);
    }

    if (S_ISDIR(vnodeStat.st_mode)) {
        stats.st_nlink++;
        inode.i_links_count = stats.st_nlink;
    }

    if (S_ISDIR(vnodeStat.st_mode) && oldDirectory != this) {
        // This cast is safe because we previously checked that the file is on
        // the same filesystem.
        ((Reference<Ext234Vnode>) vnode)->updateParent(this);
    }
    return 0;
}

Reference<Vnode> Ext234Vnode::resolve() {
    AutoLock lock(&mutex);

    if (mounted) return mounted->getRootDir();
    return this;
}

int Ext234Vnode::symlink(const char* linkTarget, const char* name) {
    AutoLock lock(&mutex);
    if (filesystem->readonly) {
        errno = EROFS;
        return -1;
    }

    if (!S_ISDIR(stats.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }

    uint64_t blockGroup = filesystem->getBlockGroup(stats.st_ino);
    ino_t ino = filesystem->createInode(blockGroup, 07777 | S_IFLNK);
    if (ino == 0) return -1;
    Reference<Ext234Vnode> symlink = filesystem->getVnode(ino);
    if (!symlink) return -1;
    symlink->updateTimestampsLocked(true, true, true);
    size_t length = strlen(linkTarget);

    if (length < 60) {
        symlink->stats.st_size = length;
        symlink->inode.i_size = length;
        memcpy(symlink->inode.i_block, linkTarget, length);
    } else {
        if (!filesystem->resizeInode(ino, &symlink->inode, length) ||
                !filesystem->writeInodeData(&symlink->inode, 0, linkTarget,
                length)) {
            return -1;
        }
    }

    return linkUnlocked(name, strcspn(name, "/"), symlink);
}

int Ext234Vnode::unlink(const char* name, int flags) {
    AutoLock lock(&mutex);
    if (filesystem->readonly) {
        errno = EROFS;
        return -1;
    }
    return unlinkUnlocked(name, flags);
}

int Ext234Vnode::unlinkUnlocked(const char* name, int flags) {
    if (!S_ISDIR(stats.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }

    DirectoryEntry entry;
    size_t nameLength = strlen(name);
    uint64_t offset = findDirectoryEntry(name, nameLength, &entry);
    if (offset == (uint64_t) -1) {
        return -1;
    }

    Reference<Vnode> vnode = filesystem->getVnode(entry.inode);
    if (!vnode) return -1;

    mode_t mode = vnode->stat().st_mode;
    if (flags) {
        if (S_ISDIR(mode) && !(flags & AT_REMOVEDIR)) {
            errno = EPERM;
            return -1;
        }
        if (!S_ISDIR(mode) &&
                !(flags & AT_REMOVEFILE || name[nameLength] == '/')) {
            errno = ENOTDIR;
            return -1;
        }
    }

    if (!vnode->onUnlink(flags == 0)) {
        return -1;
    }

    if (S_ISDIR(mode)) {
        stats.st_nlink--;
        inode.i_links_count = stats.st_nlink;
    }

    entry.inode = 0;
    if (!filesystem->writeInodeData(&inode, offset, &entry,
            sizeof(DirectoryEntry))) {
        return -1;
    }

    updateTimestamps(false, true, true);
    return 0;
}

int Ext234Vnode::unmount() {
    AutoLock lock(&mutex);

    if (!mounted) {
        errno = EINVAL;
        return -1;
    }

    if (!mounted->onUnmount()) return -1;

    delete mounted;
    mounted = nullptr;
    return 0;
}

bool Ext234Vnode::updateParent(const Reference<Ext234Vnode>& parent) {
    DirectoryEntry entry;
    uint64_t offset = findDirectoryEntry("..", 2, &entry);
    if (offset == (uint64_t) -1) return false;
    entry.inode = parent->stats.st_ino;
    inodeModified = true;
    return filesystem->writeInodeData(&inode, offset, &entry,
            sizeof(DirectoryEntry));
}

void Ext234Vnode::updateTimestamps(bool access, bool status,
        bool modification) {
    if (filesystem->readonly) return;
    Vnode::updateTimestamps(access, status, modification);
    writeTimestamps();
}

int Ext234Vnode::utimens(struct timespec atime, struct timespec mtime) {
    if (filesystem->readonly) {
        errno = EROFS;
        return -1;
    }
    Vnode::utimens(atime, mtime);

    AutoLock lock(&mutex);
    writeTimestamps();
    return 0;
}

void Ext234Vnode::writeTimestamps() {
    little_uint32_t* atimeExtra = nullptr;
    little_uint32_t* ctimeExtra = nullptr;
    little_uint32_t* mtimeExtra = nullptr;

    if (filesystem->inodeSize > 128 && inode.i_extra_isize + 128U >=
            offsetof(Inode, i_atime_extra) + sizeof(inode.i_atime_extra)) {
        atimeExtra = &inode.i_atime_extra;
    }

    if (filesystem->inodeSize > 128 && inode.i_extra_isize + 128U >=
            offsetof(Inode, i_ctime_extra) + sizeof(inode.i_ctime_extra)) {
        ctimeExtra = &inode.i_ctime_extra;
    }

    if (filesystem->inodeSize > 128 && inode.i_extra_isize + 128U >=
            offsetof(Inode, i_mtime_extra) + sizeof(inode.i_mtime_extra)) {
        mtimeExtra = &inode.i_mtime_extra;
    }

    filesystem->setTime(&stats.st_atim, &inode.i_atime, atimeExtra);
    filesystem->setTime(&stats.st_ctim, &inode.i_ctime, ctimeExtra);
    filesystem->setTime(&stats.st_mtim, &inode.i_mtime, mtimeExtra);
    inodeModified = true;
}
