/* Copyright (c) 2018 Dennis Wölfing
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

/* kernel/src/pipe.cpp
 * Pipes.
 */

#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <sys/stat.h>
#include <dennix/kernel/pipe.h>
#include <dennix/kernel/signal.h>
#include <dennix/kernel/thread.h>

class PipeVnode::Endpoint : public Vnode {
public:
    Endpoint(const Reference<PipeVnode>& pipe)
            : Vnode(S_IFIFO | S_IRUSR | S_IWUSR, 0), pipe(pipe) {}
    virtual int stat(struct stat* result);
protected:
    Reference<PipeVnode> pipe;
};

class PipeVnode::ReadEnd : public Endpoint {
public:
    ReadEnd(const Reference<PipeVnode>& pipe) : Endpoint(pipe) {}
    virtual ssize_t read(void* buffer, size_t size);
    virtual ~ReadEnd();
};

class PipeVnode::WriteEnd : public Endpoint {
public:
    WriteEnd(const Reference<PipeVnode>& pipe) : Endpoint(pipe) {}
    virtual ssize_t write(const void* buffer, size_t size);
    virtual ~WriteEnd();
};

PipeVnode::PipeVnode(Reference<Vnode>& readPipe, Reference<Vnode>& writePipe)
        : Vnode(S_IFIFO | S_IRUSR | S_IWUSR, 0) {
    readEnd = new ReadEnd(this);
    writeEnd = new WriteEnd(this);
    bufferIndex = 0;
    bytesAvailable = 0;

    readPipe = readEnd;
    writePipe = writeEnd;
}

PipeVnode::~PipeVnode() {
    assert(!readEnd);
    assert(!writeEnd);
}

int PipeVnode::Endpoint::stat(struct stat* result) {
    return pipe->stat(result);
}

ssize_t PipeVnode::ReadEnd::read(void* buffer, size_t size) {
    return pipe->read(buffer, size);
}

PipeVnode::ReadEnd::~ReadEnd() {
    AutoLock lock(&pipe->mutex);
    pipe->readEnd = nullptr;
}

ssize_t PipeVnode::WriteEnd::write(const void* buffer, size_t size) {
    return pipe->write(buffer, size);
}

PipeVnode::WriteEnd::~WriteEnd() {
    AutoLock lock(&pipe->mutex);
    pipe->writeEnd = nullptr;
}

ssize_t PipeVnode::read(void* buffer, size_t size) {
    if (size == 0) return 0;
    AutoLock lock(&mutex);

    while (bytesAvailable == 0) {
        if (!writeEnd) return 0;

        if (Signal::isPending()) {
            errno = EINTR;
            return -1;
        }

        kthread_mutex_unlock(&mutex);
        sched_yield();
        kthread_mutex_lock(&mutex);
    }

    size_t bytesRead = 0;
    char* buf = (char*) buffer;

    while (bytesAvailable > 0 && bytesRead < size) {
        buf[bytesRead] = pipeBuffer[bufferIndex];
        bufferIndex = (bufferIndex + 1) % PIPE_BUF;
        bytesAvailable--;
        bytesRead++;
    }

    updateTimestamps(true, false, false);
    return bytesRead;
}

ssize_t PipeVnode::write(const void* buffer, size_t size) {
    if (size == 0) return 0;
    AutoLock lock(&mutex);

    if (size <= PIPE_BUF) {
        while (PIPE_BUF - bytesAvailable < size && readEnd) {
            if (Signal::isPending()) {
                errno = EINTR;
                return -1;
            }

            kthread_mutex_unlock(&mutex);
            sched_yield();
            kthread_mutex_lock(&mutex);
        }
    }

    const char* buf = (const char*) buffer;
    size_t written = 0;

    while (written < size) {
        while (PIPE_BUF - bytesAvailable == 0 && readEnd) {
            if (Signal::isPending()) {
                if (written) {
                    updateTimestamps(false, true, true);
                    return written;
                }
                errno = EINTR;
                return -1;
            }

            kthread_mutex_unlock(&mutex);
            sched_yield();
            kthread_mutex_lock(&mutex);
        }

        if (!readEnd) {
            siginfo_t siginfo = {};
            siginfo.si_signo = SIGPIPE;
            siginfo.si_code = SI_KERNEL;
            Thread::current()->raiseSignal(siginfo);
            errno = EPIPE;
            return -1;
        }

        while (PIPE_BUF - bytesAvailable > 0 && written < size) {
            pipeBuffer[(bufferIndex + bytesAvailable) % PIPE_BUF] =
                    buf[written];
            written++;
            bytesAvailable++;
        }
    }

    updateTimestamps(false, true, true);
    return written;
}
