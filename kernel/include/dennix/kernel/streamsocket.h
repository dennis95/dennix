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

/* kernel/include/dennix/kernel/streamsocket.h
 * Unix domain stream sockets.
 */

#ifndef KERNEL_STREAMSOCKET_H
#define KERNEL_STREAMSOCKET_H

#include <dennix/un.h>
#include <dennix/kernel/socket.h>

class StreamSocket : public Socket, public ConstructorMayFail {
private:
    struct ConnectionMutex : public ReferenceCounted {
        kthread_mutex_t mutex = KTHREAD_MUTEX_INITIALIZER;
    };
public:
    StreamSocket(mode_t mode);
    StreamSocket(mode_t mode, const Reference<StreamSocket>& peer,
            const Reference<ConnectionMutex>& connection);
    ~StreamSocket();
    Reference<Vnode> accept(struct sockaddr* address, socklen_t* length)
            override;
    int bind(const struct sockaddr* address, socklen_t length) override;
    int connect(const struct sockaddr* address, socklen_t length) override;
    int listen(int backlog) override;
    short poll() override;
    ssize_t read(void* buffer, size_t size) override;
    ssize_t write(const void* buffer, size_t size) override;
private:
    bool addConnection(const Reference<StreamSocket>& socket);
private:
    kthread_mutex_t socketMutex;
    kthread_cond_t acceptCond;
    kthread_cond_t connectCond;
    struct sockaddr_un boundAddress;
    bool isConnected;
    bool isConnecting;
    bool isListening;
    Reference<StreamSocket> firstConnection;
    Reference<StreamSocket> lastConnection;
    Reference<StreamSocket> nextConnection;
private:
    Reference<ConnectionMutex> connectionMutex;
    kthread_cond_t receiveCond;
    kthread_cond_t sendCond;
    StreamSocket* peer;
    char* receiveBuffer;
    size_t bufferSize;
    size_t bufferIndex;
    size_t bytesAvailable;
};

#endif
