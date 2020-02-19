/* Copyright (c) 2019, 2020 Dennis Wölfing
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

/* kernel/src/devices.cpp
 * Devices.
 */

#include <errno.h>
#include <string.h>
#include <dennix/kernel/devices.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/terminaldisplay.h>

class DevFull : public Vnode {
public:
    DevFull() : Vnode(S_IFCHR | 0666, 0) {}
    virtual ssize_t read(void* /*buffer*/, size_t /*size*/) {
        return 0;
    }

    virtual ssize_t write(const void* /*buffer*/, size_t size) {
        if (size == 0) return 0;
        errno = ENOSPC;
        return -1;
    }
};

class DevNull : public Vnode {
public:
    DevNull() : Vnode(S_IFCHR | 0666, 0) {}
    virtual ssize_t read(void* /*buffer*/, size_t /*size*/) {
        return 0;
    }

    virtual ssize_t write(const void* /*buffer*/, size_t size) {
        return size;
    }
};

class DevZero : public Vnode {
public:
    DevZero() : Vnode(S_IFCHR | 0666, 0) {}
    virtual ssize_t read(void* buffer, size_t size) {
        memset(buffer, 0, size);
        return size;
    }

    virtual ssize_t write(const void* /*buffer*/, size_t size) {
        return size;
    }
};

void Devices::initialize(Reference<DirectoryVnode> rootDir) {
    Reference<DirectoryVnode> dev = xnew DirectoryVnode(rootDir, 0755, 0);
    if (rootDir->link("dev", dev) < 0 ||
            dev->link("full", xnew DevFull()) < 0 ||
            dev->link("null", xnew DevNull()) < 0 ||
            dev->link("zero", xnew DevZero()) < 0 ||
            dev->link("display", TerminalDisplay::display) < 0) {
        PANIC("Could not create /dev directory.");
    }
}
