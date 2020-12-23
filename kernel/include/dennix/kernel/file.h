/* Copyright (c) 2016, 2017, 2018, 2019, 2020 Dennis Wölfing
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

/* kernel/include/dennix/kernel/file.h
 * File Vnode.
 */

#ifndef KERNEL_FILE_H
#define KERNEL_FILE_H

#include <dennix/kernel/vnode.h>

class FileVnode : public Vnode, public ConstructorMayFail {
public:
    FileVnode(const void* data, size_t size, mode_t mode, dev_t dev);
    ~FileVnode();
    int ftruncate(off_t length) override;
    bool isSeekable() override;
    off_t lseek(off_t offset, int whence) override;
    short poll() override;
    ssize_t pread(void* buffer, size_t size, off_t offset) override;
    ssize_t pwrite(const void* buffer, size_t size, off_t offset) override;
public:
    char* data;
};

#endif
