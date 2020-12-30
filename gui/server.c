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

/* gui/server.c
 * Server.
 */

#include <err.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "connection.h"
#include "display.h"
#include "keyboard.h"
#include "mouse.h"
#include "server.h"
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
    if (fd < 0) {
        warn("accept failed");
        return;
    }

    struct Connection* connection = malloc(sizeof(struct Connection));
    if (!connection) err(1, "malloc");
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
    msg.display_width = displayRect.width;
    msg.display_height = displayRect.height;
    sendEvent(connection, GUI_EVENT_STATUS, sizeof(msg), &msg);
}

static void addConnection(struct Connection* connection) {
    connection->index = numConnections;

    numConnections++;
    if (numConnections > connectionsAllocated) {
        connections = reallocarray(connections, connectionsAllocated,
                2 * sizeof(struct Connection*));
        if (!connections) err(1, "realloc");
        pfd = reallocarray(pfd, 3 + 2 * connectionsAllocated,
                sizeof(struct pollfd));
        if (!pfd) err(1, "realloc");
        connectionsAllocated *= 2;
    }

    connections[connection->index] = connection;
    pfd[connection->index + 3].fd = connection->fd;
    pfd[connection->index + 3].events = POLLIN | POLLOUT;
    pfd[connection->index + 3].revents = 0;
}

static void closeConnection(struct Connection* connection) {
    numConnections--;
    connections[connection->index] = connections[numConnections];
    connections[connection->index]->index = connection->index;
    pfd[connection->index + 3] = pfd[numConnections + 3];

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
    if (serverFd < 0) err(1, "socket");
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;

    snprintf(addr.sun_path, sizeof(addr.sun_path), "/run/gui-%jd",
            (intmax_t) getpid());
    setenv("DENNIX_GUI_SOCKET", addr.sun_path, 1);
    unlink(addr.sun_path);
    if (bind(serverFd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        err(1, "bind");
    }
    atexit(unlinkSocket);

    if (listen(serverFd, 1) < 0) err(1, "listen");

    connectionsAllocated = 8;
    connections = malloc(connectionsAllocated * sizeof(struct Connection*));
    if (!connections) err(1, "malloc");

    pfd = malloc((3 + connectionsAllocated) * sizeof(struct pollfd));
    if (!pfd) err(1, "malloc");

    pfd[0].fd = serverFd;
    pfd[0].events = POLLIN;

    pfd[1].fd = 0;
    pfd[1].events = POLLIN;

    pfd[2].fd = mouseFd;
    pfd[2].events = POLLIN;
}

void pollEvents(void) {
    if (poll(pfd, 3 + numConnections, -1) >= 1) {
        if (pfd[0].revents & POLLIN) {
            acceptConnections();
        }

        if (pfd[1].revents & POLLIN) {
            handleKeyboard();
        }

        if (pfd[2].revents & POLLIN) {
            handleMouse();
        }

        for (size_t i = 0; i < numConnections; i++) {
            if (pfd[3 + i].revents & POLLIN) {
                if (!receiveMessage(connections[i])) {
                    closeConnection(connections[i]);
                    i--;
                    continue;
                }
            }
            if (pfd[3 + i].revents & POLLOUT &&
                    connections[i]->outputBuffered) {
                flushConnectionBuffer(connections[i]);
            } else if (pfd[3 + i].revents & POLLHUP) {
                closeConnection(connections[i]);
                i--;
            }
        }
    }
}

static void unlinkSocket(void) {
    unlink(getenv("DENNIX_GUI_SOCKET"));
}
