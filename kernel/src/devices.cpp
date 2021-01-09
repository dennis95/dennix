/* Copyright (c) 2019, 2020, 2021 Dennis Wölfing
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
#include <sys/stat.h>
#include <dennix/poll.h>
#include <dennix/kernel/devices.h>
#include <dennix/kernel/mouse.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/terminaldisplay.h>

static DevFS devFs;
Reference<DevFS> devFS(&devFs);

class CharDevice : public Vnode {
public:
    CharDevice() : Vnode(S_IFCHR | 0666, devFS->stats.st_dev) {
        stats.st_rdev = (uintptr_t) this;
    }

    short poll() override {
        return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
    }

    ssize_t write(const void* /*buffer*/, size_t size, int /*flags*/) override {
        return size;
    }
};

class DevFull : public CharDevice {
public:
    ssize_t read(void* /*buffer*/, size_t /*size*/, int /*flags*/) override {
        return 0;
    }

    ssize_t write(const void* /*buffer*/, size_t size, int /*flags*/) override {
        if (size == 0) return 0;
        errno = ENOSPC;
        return -1;
    }
};

class DevNull : public CharDevice {
public:
    ssize_t read(void* /*buffer*/, size_t /*size*/, int /*flags*/) override {
        return 0;
    }
};

class DevZero : public CharDevice {
public:
    ssize_t read(void* buffer, size_t size, int /*flags*/) override {
        memset(buffer, 0, size);
        return size;
    }
};

class DevRandom : public CharDevice {
public:
    ssize_t read(void* buffer, size_t size, int /*flags*/) override {
        arc4random_buf(buffer, size);
        return size;
    }
};

DevFS::DevFS() : DirectoryVnode(nullptr, 0755, (uintptr_t) this) {

}

void DevFS::addDevice(const char* name, const Reference<Vnode>& vnode) {
    if (DirectoryVnode::link(name, vnode) < 0) {
        PANIC("Could not add device '/dev/%s'", name);
    }
}

void DevFS::initialize(const Reference<DirectoryVnode>& rootDir) {
    parent = rootDir;
    if (rootDir->link("dev", this) < 0) {
        PANIC("Could not create /dev directory.");
    }
    addDevice("full", xnew DevFull());
    addDevice("null", xnew DevNull());
    addDevice("zero", xnew DevZero());
    addDevice("display", TerminalDisplay::display);
    Reference<Vnode> random = xnew DevRandom();
    addDevice("random", random);
    addDevice("urandom", random);

    // Update the /dev/display timestamp to avoid a 1970 timestamp.
    TerminalDisplay::display->updateTimestampsLocked(true, true, true);
}

// Prevent the user from deleting devices or otherwise modifying /dev.

int DevFS::link(const char* /*name*/, const Reference<Vnode>& /*vnode*/) {
    errno = EROFS;
    return -1;
}

int DevFS::mkdir(const char* /*name*/, mode_t /*mode*/) {
    errno = EROFS;
    return -1;
}

Reference<Vnode> DevFS::open(const char* name, int flags, mode_t /*mode*/) {
    size_t length = strcspn(name, "/");
    Reference<Vnode> vnode = getChildNode(name, length);
    if (!vnode) {
        return nullptr;
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

int DevFS::rename(const Reference<Vnode>& /*oldDirectory*/,
        const char* /*oldName*/, const char* /*newName*/) {
    errno = EROFS;
    return -1;
}

int DevFS::unlink(const char* /*path*/, int /*flags*/) {
    errno = EROFS;
    return -1;
}
