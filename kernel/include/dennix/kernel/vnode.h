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

/* kernel/include/dennix/kernel/vnode.h
 * Vnode class.
 */

#ifndef KERNEL_VNODE_H
#define KERNEL_VNODE_H

#include <sys/types.h>

class Vnode {
public:
    virtual bool isSeekable();
    virtual Vnode* openat(const char* path, int flags, mode_t mode);
    virtual ssize_t pread(void* buffer, size_t size, off_t offset);
    virtual ssize_t read(void* buffer, size_t size);
    virtual ssize_t write(const void* buffer, size_t size);
    virtual ~Vnode() {}
};

#endif
