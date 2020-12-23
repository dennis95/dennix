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
#include <stdlib.h>
#include <string.h>
#include <dennix/poll.h>
#include <dennix/kernel/devices.h>
#include <dennix/kernel/mouse.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/terminaldisplay.h>

class CharDevice : public Vnode {
public:
    CharDevice() : Vnode(S_IFCHR | 0666, 0) {}
    short poll() override {
        return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
    }

    ssize_t write(const void* /*buffer*/, size_t size) override {
        return size;
    }
};

class DevFull : public CharDevice {
public:
    ssize_t read(void* /*buffer*/, size_t /*size*/) override {
        return 0;
    }

    ssize_t write(const void* /*buffer*/, size_t size) override {
        if (size == 0) return 0;
        errno = ENOSPC;
        return -1;
    }
};

class DevNull : public CharDevice {
public:
    ssize_t read(void* /*buffer*/, size_t /*size*/) override {
        return 0;
    }
};

class DevZero : public CharDevice {
public:
    ssize_t read(void* buffer, size_t size) override {
        memset(buffer, 0, size);
        return size;
    }
};

class DevRandom : public CharDevice {
public:
    ssize_t read(void* buffer, size_t size) override {
        arc4random_buf(buffer, size);
        return size;
    }
};

void Devices::initialize(Reference<DirectoryVnode> rootDir) {
    Reference<DirectoryVnode> dev = xnew DirectoryVnode(rootDir, 0755, 0);
    Reference<Vnode> random = xnew DevRandom();
    if (rootDir->link("dev", dev) < 0 ||
            dev->link("full", xnew DevFull()) < 0 ||
            dev->link("null", xnew DevNull()) < 0 ||
            dev->link("zero", xnew DevZero()) < 0 ||
            dev->link("display", TerminalDisplay::display) < 0 ||
            dev->link("random", random) < 0 ||
            dev->link("urandom", random) < 0 ||
            (mouseDevice && dev->link("mouse", mouseDevice) < 0)) {
        PANIC("Could not create /dev directory.");
    }
}
