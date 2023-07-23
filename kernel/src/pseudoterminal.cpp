/* Copyright (c) 2021, 2023 Dennis Wölfing
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

/* kernel/src/pseudoterminal.cpp
 * Pseudo terminals.
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <dennix/poll.h>
#include <dennix/kernel/devices.h>
#include <dennix/kernel/dynarray.h>
#include <dennix/kernel/pseudoterminal.h>

#define BUFFER_SIZE (1024 * 1024) // 1 MiB

class PseudoTerminal : public Terminal {
public:
    PseudoTerminal();
    ~PseudoTerminal();
    NOT_COPYABLE(PseudoTerminal);
    NOT_MOVABLE(PseudoTerminal);

    short poll() override;
    short pollController();
    ssize_t readController(void* buffer, size_t size, int flags);
    ssize_t writeController(const void* buffer, size_t size, int flags);
protected:
    bool getTtyPath(char* buffer, size_t size) override;
    void output(const char* buffer, size_t size) override;
public:
    unsigned int number;
private:
    char controllerBuffer[BUFFER_SIZE];
    size_t bufferIndex;
    size_t bytesAvailable;
    kthread_cond_t controllerReadCond;
    kthread_cond_t outputCond;
};

class PtController : public Vnode {
public:
    PtController(const Reference<PseudoTerminal>& pts);
    ~PtController();
    NOT_COPYABLE(PtController);
    NOT_MOVABLE(PtController);

    int devctl(int command, void* restrict data, size_t size,
            int* restrict info) override;
    int isatty() override;
    short poll() override;
    ssize_t read(void* buffer, size_t size, int flags) override;
    int tcgetattr(struct termios* result) override;
    int tcsetattr(int flags, const struct termios* termios) override;
    ssize_t write(const void* buffer, size_t size, int flags) override;
private:
    Reference<PseudoTerminal> pts;
};

static DynamicArray<PseudoTerminal*, unsigned int> pseudoTerminals;
static kthread_mutex_t ptsMutex = KTHREAD_MUTEX_INITIALIZER;

DevPtmx::DevPtmx() : Vnode(S_IFCHR | 0666, DevFS::dev) {

}

Reference<Vnode> DevPtmx::resolve() {
    Reference<PseudoTerminal> pts = new PseudoTerminal();
    if (!pts || pts->number == (unsigned int) -1) {
        return nullptr;
    }

    Reference<PtController> controller = new PtController(pts);
    return controller;
}

DevPts::DevPts() : Vnode(S_IFDIR | 0755, DevFS::dev) {

}

Reference<Vnode> DevPts::getChildNode(const char* name) {
    if (strcmp(name, ".") == 0) {
        return this;
    } else if (strcmp(name, "..") == 0) {
        return devFS.getRootDir();
    }

    char* end;
    unsigned long value = strtoul(name, &end, 10);

    AutoLock lock(&ptsMutex);
    if (value > INT_MAX || *end || value >= pseudoTerminals.allocatedSize) {
        errno = ENOENT;
        return nullptr;
    }

    Reference<PseudoTerminal> pts = pseudoTerminals[value];
    if (!pts) {
        errno = ENOENT;
        return nullptr;
    }
    return pts;
}

Reference<Vnode> DevPts::getChildNode(const char* name, size_t length) {
    char* nameCopy = strndup(name, length);
    if (!nameCopy) return nullptr;
    Reference<Vnode> result = getChildNode(nameCopy);
    free(nameCopy);
    return result;
}

size_t DevPts::getDirectoryEntries(void** buffer, int /*flags*/) {
    AutoLock lock(&ptsMutex);

    size_t maxEntrySize = ALIGNUP(sizeof(posix_dent) + 11, alignof(posix_dent));
    size_t maxSize = pseudoTerminals.allocatedSize * maxEntrySize +
            ALIGNUP(sizeof(posix_dent) + 2, alignof(posix_dent)) +
            ALIGNUP(sizeof(posix_dent) + 3, alignof(posix_dent));

    void* buf = malloc(maxSize);
    if (!buf) return 0;
    size_t sizeUsed = 0;

    posix_dent* dent = (posix_dent*) buf;
    dent->d_ino = stats.st_ino;
    dent->d_reclen = ALIGNUP(sizeof(posix_dent) + 2, alignof(posix_dent));
    dent->d_type = DT_DIR;
    strcpy(dent->d_name, ".");
    sizeUsed += dent->d_reclen;

    dent = (posix_dent*) ((char*) dent + dent->d_reclen);
    dent->d_ino = devFS.getRootDir()->stat().st_ino;
    dent->d_reclen = ALIGNUP(sizeof(posix_dent) + 3, alignof(posix_dent));
    dent->d_type = DT_DIR;
    strcpy(dent->d_name, "..");
    sizeUsed += dent->d_reclen;

    dent = (posix_dent*) ((char*) dent + dent->d_reclen);
    for (Reference<PseudoTerminal> pts : pseudoTerminals) {
        dent->d_ino = pts->stat().st_ino;
        int length = sprintf(dent->d_name, "%u", pts->number);
        dent->d_reclen = ALIGNUP(sizeof(posix_dent) + length + 1,
                alignof(posix_dent));
        dent->d_type = DT_CHR;
        sizeUsed += dent->d_reclen;
        dent = (posix_dent*) ((char*) dent + dent->d_reclen);
    }

    *buffer = realloc(buf, sizeUsed);
    if (!*buffer) *buffer = buf;
    return sizeUsed;
}

Reference<Vnode> DevPts::open(const char* name, int flags, mode_t /*mode*/) {
    size_t length = strcspn(name, "/");
    Reference<Vnode> vnode = getChildNode(name, length);
    if (!vnode) {
        return nullptr;
    } else if (flags & O_EXCL) {
        errno = EEXIST;
        return nullptr;
    }

    return vnode;
}

PseudoTerminal::PseudoTerminal() : Terminal(DevFS::dev) {
    AutoLock lock(&ptsMutex);
    number = pseudoTerminals.add(this);

    bufferIndex = 0;
    bytesAvailable = 0;
    controllerReadCond = KTHREAD_COND_INITIALIZER;
    outputCond = KTHREAD_COND_INITIALIZER;
}

PseudoTerminal::~PseudoTerminal() {
    AutoLock lock(&ptsMutex);
    if (number != (unsigned int) -1) {
        pseudoTerminals.remove(number);
    }
}

bool PseudoTerminal::getTtyPath(char* buffer, size_t size) {
    return (size_t) snprintf(buffer, size, "/dev/pts/%u", number) < size;
}

// TODO: The output function has to block when the buffer is full but this can
// cause a deadlock if data written by the controller is echoed. Fixing this is
// difficult because not all data written by the controller causes output and
// the output might also be larger than the written data. This would mean that
// the terminal would have to precalculate the output before performing any
// actions based on the written data. Also partial writes would be problematic
// since the data written to the terminal buffer and the output would have to be
// consistent. Right now we work around this issue by making the buffer large
// enough that the buffer is unlikely to get full.
void PseudoTerminal::output(const char* buffer, size_t size) {
    const char* buf = (const char*) buffer;
    size_t written = 0;

    while (written < size) {
        while (BUFFER_SIZE - bytesAvailable == 0) {
            if (kthread_cond_sigwait(&outputCond, &mutex) == EINTR) {
                // TODO: We are losing data here.
                return;
            }
        }

        while (BUFFER_SIZE - bytesAvailable > 0 && written < size) {
            controllerBuffer[(bufferIndex + bytesAvailable) % BUFFER_SIZE] =
                    buf[written];
            written++;
            bytesAvailable++;
        }
        kthread_cond_broadcast(&controllerReadCond);
    }
}

short PseudoTerminal::poll() {
    AutoLock lock(&mutex);
    short result = 0;
    if (dataAvailable()) result |= POLLIN | POLLRDNORM;
    if (hungup) {
        result |= POLLHUP;
    } else if (bytesAvailable < BUFFER_SIZE) {
        result |= POLLOUT | POLLWRNORM;
    }
    return result;
}

short PseudoTerminal::pollController() {
    AutoLock lock(&mutex);
    short result = 0;
    if (bytesAvailable) result |= POLLIN | POLLRDNORM;
    if (canWriteBuffer()) result |= POLLOUT | POLLWRNORM;
    return result;
}

ssize_t PseudoTerminal::readController(void* buffer, size_t size, int flags) {
    if (size == 0) return 0;
    AutoLock lock(&mutex);

    while (bytesAvailable == 0) {
        if (flags & O_NONBLOCK) {
            errno = EAGAIN;
            return -1;
        }

        if (kthread_cond_sigwait(&controllerReadCond, &mutex) == EINTR) {
            errno = EINTR;
            return -1;
        }
    }

    size_t bytesRead = 0;
    char* buf = (char*) buffer;

    while (bytesAvailable > 0 && bytesRead < size) {
        buf[bytesRead] = controllerBuffer[bufferIndex];
        bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
        bytesAvailable--;
        bytesRead++;
    }

    kthread_cond_broadcast(&outputCond);
    updateTimestamps(true, false, false);
    return bytesRead;
}

ssize_t PseudoTerminal::writeController(const void* buffer, size_t size,
        int /*flags*/) {
    AutoLock lock(&mutex);
    if (!(termio.c_cflag & CREAD)) return size;

    const char* buf = (const char*) buffer;
    for (size_t i = 0; i < size; i++) {
        handleCharacter(buf[i]);
    }

    return size;
}

PtController::PtController(const Reference<PseudoTerminal>& pts)
        : Vnode(S_IFCHR | 0666, DevFS::dev), pts(pts) {

}

PtController::~PtController() {
    pts->hangup();
}

int PtController::devctl(int command, void* restrict data, size_t size,
        int* restrict info) {
    return pts->devctl(command, data, size, info);
}

int PtController::isatty() {
    return 1;
}

short PtController::poll() {
    return pts->pollController();
}

ssize_t PtController::read(void* buffer, size_t size, int flags) {
    return pts->readController(buffer, size, flags);
}

int PtController::tcgetattr(struct termios* result) {
    return pts->tcgetattr(result);
}

int PtController::tcsetattr(int flags, const struct termios* termios) {
    return pts->tcsetattr(flags, termios);
}

ssize_t PtController::write(const void* buffer, size_t size, int flags) {
    return pts->writeController(buffer, size, flags);
}
