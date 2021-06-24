/* Copyright (c) 2020, 2021 Dennis Wölfing
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

/* gui/server.c
 * Server.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "connection.h"
#include "window.h"

static struct Connection** connections;
static size_t connectionsAllocated;
static size_t numConnections;
static struct pollfd* pfd;
static int serverFd;

static void acceptConnections(void);
static void addConnection(struct Connection* connection);
static void unlinkSocket(void);

static void acceptConnections(void) {
    int fd = accept4(serverFd, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (fd < 0) return;

    struct Connection* connection = malloc(sizeof(struct Connection));
    if (!connection) dxui_panic(context, "malloc");
    connection->fd = fd;
    connection->windows = NULL;
    connection->windowsAllocated = 0;
    connection->headerReceived = 0;
    connection->messageBuffer = NULL;
    connection->messageReceived = 0;
    connection->outputBuffer = NULL;
    connection->outputBuffered = 0;
    connection->outputBufferOffset = 0;
    connection->outputBufferSize = 0;
    addConnection(connection);

    struct gui_event_status msg;
    msg.flags = 0;
    msg.display_width = guiDim.width;
    msg.display_height = guiDim.height;
    sendEvent(connection, GUI_EVENT_STATUS, sizeof(msg), &msg);
}

static void addConnection(struct Connection* connection) {
    connection->index = numConnections;

    numConnections++;
    if (numConnections > connectionsAllocated) {
        connections = reallocarray(connections, connectionsAllocated,
                2 * sizeof(struct Connection*));
        if (!connections) dxui_panic(context, "realloc");
        pfd = reallocarray(pfd, 1 + 2 * connectionsAllocated + DXUI_POLL_NFDS,
                sizeof(struct pollfd));
        if (!pfd) dxui_panic(context, "realloc");
        connectionsAllocated *= 2;
    }

    connections[connection->index] = connection;
    pfd[connection->index + 1].fd = connection->fd;
    pfd[connection->index + 1].events = POLLIN | POLLOUT;
    pfd[connection->index + 1].revents = 0;
}

void broadcastStatusEvent(void) {
    struct gui_event_status msg;
    msg.flags = 0;
    msg.display_width = guiDim.width;
    msg.display_height = guiDim.height;

    for (size_t i = 0; i < numConnections; i++) {
        sendEvent(connections[i], GUI_EVENT_STATUS, sizeof(msg), &msg);
    }
}

static void closeConnection(struct Connection* connection) {
    numConnections--;
    connections[connection->index] = connections[numConnections];
    connections[connection->index]->index = connection->index;
    pfd[connection->index + 1] = pfd[numConnections + 1];

    for (size_t i = 0; i < connection->windowsAllocated; i++) {
        if (connection->windows[i]) {
            closeWindow(connection->windows[i]);
        }
    }

    close(connection->fd);
    free(connection->messageBuffer);
    free(connection->outputBuffer);
    free(connection);
}

void initializeServer(void) {
    serverFd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (serverFd < 0) dxui_panic(context, "socket");
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;

    snprintf(addr.sun_path, sizeof(addr.sun_path), "/run/gui-%jd",
            (intmax_t) getpid());
    setenv("DENNIX_GUI_SOCKET", addr.sun_path, 1);
    unlink(addr.sun_path);
    if (bind(serverFd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        dxui_panic(context, "bind");
    }
    atexit(unlinkSocket);

    if (listen(serverFd, 1) < 0) dxui_panic(context, "listen");

    connectionsAllocated = 8;
    connections = malloc(connectionsAllocated * sizeof(struct Connection*));
    if (!connections) dxui_panic(context, "malloc");

    pfd = malloc((1 + connectionsAllocated + DXUI_POLL_NFDS) *
            sizeof(struct pollfd));
    if (!pfd) dxui_panic(context, "malloc");

    pfd[0].fd = serverFd;
    pfd[0].events = POLLIN;
}

void pollEvents(void) {
    int result = dxui_poll(context, pfd, 1 + numConnections, -1);
    if (result < 0 && errno != EINTR) {
        for (struct Window* win = topWindow; win; win = win->below) {
            closeWindow(win);
        }
        exit(0);
    } else if (result >= 1) {
        if (pfd[0].revents & POLLIN) {
            acceptConnections();
        }

        for (size_t i = 0; i < numConnections; i++) {
            if (pfd[1 + i].revents & POLLIN) {
                if (!receiveMessage(connections[i])) {
                    closeConnection(connections[i]);
                    i--;
                    continue;
                }
            }
            if (pfd[1 + i].revents & POLLOUT &&
                    connections[i]->outputBuffered) {
                flushConnectionBuffer(connections[i]);
            } else if (pfd[1 + i].revents & POLLHUP) {
                closeConnection(connections[i]);
                i--;
            }
        }
    }
}

static void unlinkSocket(void) {
    unlink(getenv("DENNIX_GUI_SOCKET"));
}
