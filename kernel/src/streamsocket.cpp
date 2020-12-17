/* Copyright (c) 2020 Dennis Wölfing
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

/* kernel/src/streamsocket.cpp
 * Unix domain stream sockets.
 */

#include <errno.h>
#include <sched.h>
#include <string.h>
#include <sys/stat.h>
#include <dennix/poll.h>
#include <dennix/un.h>
#include <dennix/kernel/process.h>
#include <dennix/kernel/streamsocket.h>

#define BUFFER_SIZE (64 * 1024) // 64 KiB

StreamSocket::StreamSocket(mode_t mode) : Socket(SOCK_STREAM, mode) {
    socketMutex = KTHREAD_MUTEX_INITIALIZER;
    acceptCond = KTHREAD_COND_INITIALIZER;
    connectCond = KTHREAD_COND_INITIALIZER;
    boundAddress.sun_family = AF_UNSPEC;
    isConnected = false;
    isConnecting = false;
    isListening = false;

    receiveCond = KTHREAD_COND_INITIALIZER;
    sendCond = KTHREAD_COND_INITIALIZER;
    peer = nullptr;
    receiveBuffer = nullptr;
    bufferIndex = 0;
    bufferSize = 0;
    bytesAvailable = 0;
}

StreamSocket::StreamSocket(mode_t mode, const Reference<StreamSocket>& peer,
        const Reference<ConnectionMutex>& connectionMutex)
        : StreamSocket(mode) {
    isConnected = true;
    this->peer = (StreamSocket*) peer;
    this->connectionMutex = connectionMutex;
    bufferSize = BUFFER_SIZE;
    receiveBuffer = new char[bufferSize];
    if (!receiveBuffer) FAIL_CONSTRUCTOR;
}

StreamSocket::~StreamSocket() {
    if (isConnected) {
        kthread_mutex_lock(&connectionMutex->mutex);
        if (peer) {
            peer->peer = nullptr;
            kthread_cond_broadcast(&peer->receiveCond);
            kthread_cond_broadcast(&peer->sendCond);
        }
        kthread_mutex_unlock(&connectionMutex->mutex);
        delete receiveBuffer;
    }

    while (firstConnection) {
        kthread_cond_broadcast(&firstConnection->connectCond);
        Reference<StreamSocket> connection = firstConnection;
        firstConnection = firstConnection->nextConnection;
        connection->nextConnection = nullptr;
    }
}

Reference<Vnode> StreamSocket::accept(struct sockaddr* address,
        socklen_t* length) {
    AutoLock lock(&socketMutex);

    if (!isListening) {
        errno = EINVAL;
        return nullptr;
    }

    while (!firstConnection) {
        if (kthread_cond_sigwait(&acceptCond, &socketMutex) == EINTR) {
            errno = EINTR;
            return nullptr;
        }
    }

    Reference<StreamSocket> incoming = firstConnection;
    firstConnection = firstConnection->nextConnection;
    if (!firstConnection) {
        lastConnection = nullptr;
    }
    incoming->nextConnection = nullptr;

    Reference<ConnectionMutex> connectionMutex;
    Reference<StreamSocket> newSocket;
    char* buffer;

    if (!(connectionMutex = new ConnectionMutex()) ||
            !(newSocket = new StreamSocket(stat().st_mode, incoming,
            connectionMutex)) ||
            !(buffer = new char[BUFFER_SIZE])) {
        kthread_mutex_lock(&incoming->socketMutex);
        incoming->isConnecting = false;
        kthread_cond_broadcast(&incoming->connectCond);
        kthread_mutex_unlock(&incoming->socketMutex);
        return nullptr;
    }

    kthread_mutex_lock(&incoming->socketMutex);
    incoming->peer = (StreamSocket*) newSocket;
    incoming->connectionMutex = connectionMutex;
    incoming->isConnected = true;
    incoming->isConnecting = false;
    incoming->receiveBuffer = buffer;
    incoming->bufferSize = BUFFER_SIZE;
    struct sockaddr_un peerAddr = incoming->boundAddress;
    kthread_cond_broadcast(&incoming->connectCond);
    kthread_mutex_unlock(&incoming->socketMutex);

    if (address) {
        size_t addressSize = peerAddr.sun_family == AF_UNSPEC ? 0 :
                sizeof(struct sockaddr_un);
        if (*length < 0) *length = 0;
        size_t copySize = (size_t) *length < addressSize ? *length :
                addressSize;
        memcpy(address, &peerAddr, copySize);
        *length = copySize;
    }

    return newSocket;
}

bool StreamSocket::addConnection(const Reference<StreamSocket>& socket) {
    AutoLock lock(&socketMutex);

    if (!isListening) return false;
    if (lastConnection) {
        lastConnection->nextConnection = socket;
    } else {
        firstConnection = socket;
    }
    lastConnection = socket;

    kthread_cond_signal(&acceptCond);
    return true;
}

int StreamSocket::bind(const struct sockaddr* address, socklen_t length) {
    AutoLock lock(&socketMutex);

    if (!address) {
        errno = EDESTADDRREQ;
        return -1;
    }

    if (boundAddress.sun_family != AF_UNSPEC) {
        errno = EINVAL;
        return -1;
    }

    if (isConnected || isConnecting) {
        errno = EISCONN;
        return -1;
    }

    if (address->sa_family != AF_UNIX) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    if (length != sizeof(struct sockaddr_un)) {
        errno = EINVAL;
        return -1;
    }

    const struct sockaddr_un* addr = (const struct sockaddr_un*) address;
    // Check that the string is null terminated.
    for (size_t i = 0; i < length - sizeof(sa_family_t); i++) {
        if (!addr->sun_path[i]) break;
        if (i == length - sizeof(sa_family_t) - 1) {
            errno = EAFNOSUPPORT;
            return -1;
        }
    }

    Reference<Vnode> directory;
    if (addr->sun_path[0] == '\0') {
        errno = ENOENT;
        return -1;
    } else if (addr->sun_path[0] == '/') {
        directory = Process::current()->rootFd->vnode;
    } else {
        directory = Process::current()->cwdFd->vnode;
    }

    const char* lastComponent;
    directory = resolvePathExceptLastComponent(directory, addr->sun_path,
            &lastComponent);
    if (!directory) return -1;

    if (directory->link(lastComponent, this) < 0) {
        if (errno == EEXIST) errno = EADDRINUSE;
        return -1;
    }

    boundAddress = *addr;
    return 0;
}

int StreamSocket::connect(const struct sockaddr* address, socklen_t length) {
    AutoLock lock(&socketMutex);

    if (isConnecting) {
        errno = EALREADY;
        return -1;
    }

    if (isConnected) {
        errno = EISCONN;
        return -1;
    }

    if (isListening) {
        errno = EOPNOTSUPP;
        return -1;
    }

    if (address->sa_family != AF_UNIX) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    if (length != sizeof(struct sockaddr_un)) {
        errno = EINVAL;
        return -1;
    }

    const struct sockaddr_un* addr = (const struct sockaddr_un*) address;
    // Check that the string is null terminated.
    for (size_t i = 0; i < length - sizeof(sa_family_t); i++) {
        if (!addr->sun_path[i]) break;
        if (i == length - sizeof(sa_family_t) - 1) {
            errno = EAFNOSUPPORT;
            return -1;
        }
    }

    {
        Reference<Vnode> directory;
        if (addr->sun_path[0] == '/') {
            directory = Process::current()->rootFd->vnode;
        } else {
            directory = Process::current()->cwdFd->vnode;
        }

        Reference<Vnode> vnode = resolvePath(directory, addr->sun_path);
        if (!vnode) return -1;
        if (!S_ISSOCK(vnode->stat().st_mode)) {
            errno = ECONNREFUSED;
            return -1;
        }
        Reference<Socket> socket = (Reference<Socket>) vnode;
        if (socket->type != SOCK_STREAM) {
            errno = EPROTOTYPE;
            return -1;
        }
        Reference<StreamSocket> streamSocket = (Reference<StreamSocket>) socket;

        // Attempting to connect a socket to itself would deadlock.
        if (streamSocket == this || !streamSocket->addConnection(this)) {
            errno = ECONNREFUSED;
            return -1;
        }
        // We now need to drop the reference to the socket to allow it to be
        // destroyed asynchronously while we are waiting for a connection.
    }

    isConnecting = true;
    while (isConnecting) {
        if (kthread_cond_sigwait(&connectCond, &socketMutex) == EINTR) {
            // The connection will be established asynchronously.
            errno = EINTR;
            return -1;
        }
    }

    if (!isConnected) {
        errno = ECONNREFUSED;
        return -1;
    }

    return 0;
}

int StreamSocket::listen(int /*backlog*/) {
    AutoLock lock(&socketMutex);

    if (boundAddress.sun_family == AF_UNSPEC) {
        errno = EDESTADDRREQ;
        return -1;
    }

    if (isConnected || isConnecting) {
        errno = EINVAL;
        return -1;
    }

    isListening = true;
    return 0;
}

short StreamSocket::poll() {
    AutoLock lock(&socketMutex);
    short result = 0;

    if (isListening && firstConnection) {
        result |= POLLIN | POLLRDNORM;
    } else if (isConnected) {
        AutoLock lock(&connectionMutex->mutex);

        if (bytesAvailable) {
            result |= POLLIN | POLLRDNORM;
        }

        if (peer) {
            if (peer->bytesAvailable < peer->bufferSize) {
                result |= POLLOUT | POLLWRNORM;
            }
        } else {
            result |= POLLHUP;
        }
    }

    return result;
}

ssize_t StreamSocket::read(void* buffer, size_t size) {
    {
        AutoLock lock(&socketMutex);

        while (isConnecting) {
            if (kthread_cond_sigwait(&connectCond, &socketMutex) == EINTR) {
                errno = EINTR;
                return -1;
            }
        }

        if (!isConnected) {
            errno = ENOTCONN;
            return -1;
        }
    }

    AutoLock lock(&connectionMutex->mutex);

    while (bytesAvailable == 0) {
        if (!peer) {
            errno = ECONNRESET;
            return -1;
        }

        if (kthread_cond_sigwait(&receiveCond, &connectionMutex->mutex) ==
                EINTR) {
            errno = EINTR;
            return -1;
        }
    }

    size_t bytesRead = 0;
    char* buf = (char*) buffer;

    while (bytesAvailable > 0 && bytesRead < size) {
        buf[bytesRead] = receiveBuffer[bufferIndex];
        bufferIndex = (bufferIndex + 1) % bufferSize;
        bytesAvailable--;
        bytesRead++;
    }

    if (peer) {
        kthread_cond_broadcast(&peer->sendCond);
    }
    updateTimestamps(true, false, false);
    return bytesRead;
}

ssize_t StreamSocket::write(const void* buffer, size_t size) {
    {
        AutoLock lock(&socketMutex);

        while (isConnecting) {
            if (kthread_cond_sigwait(&connectCond, &socketMutex) == EINTR) {
                errno = EINTR;
                return -1;
            }
        }

        if (!isConnected) {
            errno = ENOTCONN;
            return -1;
        }
    }

    AutoLock lock(&connectionMutex->mutex);
    const char* buf = (const char*) buffer;
    size_t written = 0;

    while (written < size) {
        while (peer && peer->bufferSize - peer->bytesAvailable == 0) {
            if (kthread_cond_sigwait(&sendCond, &connectionMutex->mutex) ==
                    EINTR) {
                if (written) {
                    updateTimestamps(false, true, true);
                    return written;
                }
                errno = EINTR;
                return -1;
            }
        }

        if (!peer) {
            siginfo_t siginfo = {};
            siginfo.si_signo = SIGPIPE;
            siginfo.si_code = SI_KERNEL;
            Thread::current()->raiseSignal(siginfo);
            errno = EPIPE;
            return -1;
        }

        while (peer && peer->bufferSize - peer->bytesAvailable > 0 &&
                written < size) {
            peer->receiveBuffer[(peer->bufferIndex + peer->bytesAvailable) %
                    peer->bufferSize] = buf[written];
            written++;
            peer->bytesAvailable++;
        }
        kthread_cond_broadcast(&peer->receiveCond);
    }

    updateTimestampsLocked(false, true, true);
    return size;
}
