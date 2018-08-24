/* Copyright (c) 2016, 2017, 2018 Dennis Wölfing
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

/* kernel/include/dennix/kernel/filedescription.h
 * FileDescription class.
 */

#ifndef KERNEL_FILEDESCRIPTION_H
#define KERNEL_FILEDESCRIPTION_H

#include <dennix/kernel/vnode.h>

class FileDescription : public ReferenceCounted {
public:
    FileDescription(const Reference<Vnode>& vnode, int flags);
    int fcntl(int cmd, int param);
    off_t lseek(off_t offset, int whence);
    Reference<FileDescription> openat(const char* path, int flags,
            mode_t mode);
    ssize_t read(void* buffer, size_t size);
    ssize_t readdir(unsigned long offset, void* buffer, size_t size);
    int tcgetattr(struct termios* result);
    int tcsetattr(int flags, const struct termios* termio);
    ssize_t write(const void* buffer, size_t size);
public:
    Reference<Vnode> vnode;
private:
    off_t offset;
    int flags;
};

#endif
