/* Copyright (c) 2016, 2017, 2018, 2019, 2020, 2021 Dennis Wölfing
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

/* kernel/include/dennix/kernel/vnode.h
 * Vnode class.
 */

#ifndef KERNEL_VNODE_H
#define KERNEL_VNODE_H

#include <sys/socket.h>
#include <sys/types.h>
#include <dennix/stat.h>
#include <dennix/kernel/kthread.h>
#include <dennix/kernel/refcount.h>

class Vnode : public ReferenceCounted {
public:
    virtual Reference<Vnode> accept(struct sockaddr* address,
            socklen_t* length, int fileFlags);
    virtual int bind(const struct sockaddr* address, socklen_t length,
            int flags);
    virtual int chmod(mode_t mode);
    virtual int chown(uid_t uid, gid_t gid);
    virtual int connect(const struct sockaddr* address, socklen_t length,
            int flags);
    virtual int devctl(int command, void* restrict data, size_t size,
            int* restrict info);
    virtual int ftruncate(off_t length);
    virtual Reference<Vnode> getChildNode(const char* path);
    virtual Reference<Vnode> getChildNode(const char* path, size_t length);
    virtual size_t getDirectoryEntries(void** buffer);
    virtual char* getLinkTarget();
    virtual int isatty();
    virtual bool isSeekable();
    virtual int link(const char* name, const Reference<Vnode>& vnode);
    virtual int listen(int backlog);
    virtual off_t lseek(off_t offset, int whence);
    virtual int mkdir(const char* name, mode_t mode);
    virtual void onLink();
    virtual bool onUnlink();
    virtual Reference<Vnode> open(const char* name, int flags, mode_t mode);
    virtual short poll();
    virtual ssize_t pread(void* buffer, size_t size, off_t offset, int flags);
    virtual ssize_t pwrite(const void* buffer, size_t size, off_t offset,
                int flags);
    virtual ssize_t read(void* buffer, size_t size, int flags);
    virtual ssize_t readlink(char* buffer, size_t size);
    virtual int rename(const Reference<Vnode>& oldDirectory,
            const char* oldName, const char* newName);
    virtual int stat(struct stat* result);
    struct stat stat();
    virtual int tcgetattr(struct termios* result);
    virtual int tcsetattr(int flags, const struct termios* termio);
    virtual int unlink(const char* name, int flags);
    void updateTimestampsLocked(bool access, bool status, bool modification);
    virtual int utimens(struct timespec atime, struct timespec mtime);
    virtual ssize_t write(const void* buffer, size_t size, int flags);
    virtual ~Vnode();
protected:
    Vnode(mode_t mode, dev_t dev);
    void updateTimestamps(bool access, bool status, bool modification);
public:
    kthread_mutex_t mutex;
    struct stat stats;
};

Reference<Vnode> resolvePath(const Reference<Vnode>& vnode, const char* path,
        bool followFinalSymlink = true);
Reference<Vnode> resolvePathExceptLastComponent(const Reference<Vnode>& vnode,
        const char* path, const char** lastComponent,
        bool followFinalSymlink = false);

#endif
