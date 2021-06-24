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

/* gui/connection.c
 * Connection.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "connection.h"
#include "window.h"

static struct Window* getWindow(struct Connection* conn, unsigned int windowId);
static void handleMessage(struct Connection* conn, unsigned int type,
        size_t length, void* msg);
static bool sendOutput(struct Connection* conn, const void* buffer,
        size_t size);

static void handleCloseWindow(struct Connection* conn, size_t length,
        struct gui_msg_close_window* msg);
static void handleCreateWindow(struct Connection* conn, size_t length,
        struct gui_msg_create_window* msg);
static void handleHideWindow(struct Connection* conn, size_t length,
        struct gui_msg_hide_window* msg);
static void handleRedrawWindow(struct Connection* conn, size_t length,
        struct gui_msg_redraw_window* msg);
static void handleRedrawWindowPart(struct Connection* conn, size_t length,
        struct gui_msg_redraw_window_part* msg);
static void handleResizeWindow(struct Connection* conn, size_t length,
        struct gui_msg_resize_window* msg);
static void handleSetWindowBackground(struct Connection* conn, size_t length,
        struct gui_msg_set_window_background* msg);
static void handleSetWindowCursor(struct Connection* conn, size_t length,
        struct gui_msg_set_window_cursor* msg);
static void handleSetWindowTitle(struct Connection* conn, size_t length,
        struct gui_msg_set_window_title* msg);
static void handleShowWindow(struct Connection* conn, size_t length,
        struct gui_msg_show_window* msg);

bool flushConnectionBuffer(struct Connection* conn) {
    while (conn->outputBuffered) {
        size_t copySize = conn->outputBufferSize - conn->outputBufferOffset;
        if (conn->outputBuffered < copySize) copySize = conn->outputBuffered;
        ssize_t written = write(conn->fd,
                conn->outputBuffer + conn->outputBufferOffset, copySize);
        if (written < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
            return false;
        }
        conn->outputBufferOffset = (conn->outputBufferOffset + written) %
                conn->outputBufferSize;
        conn->outputBuffered -= written;
    }

    conn->outputBufferOffset = 0;
    return true;
}

static struct Window* getWindow(struct Connection* conn,
        unsigned int windowId) {
    if (windowId >= conn->windowsAllocated) return NULL;
    return conn->windows[windowId];
}

static void handleMessage(struct Connection* conn, unsigned int type,
        size_t length, void* msg) {
    switch (type) {
    case GUI_MSG_CLOSE_WINDOW:
        handleCloseWindow(conn, length, msg);
        break;
    case GUI_MSG_CREATE_WINDOW:
        handleCreateWindow(conn, length, msg);
        break;
    case GUI_MSG_HIDE_WINDOW:
        handleHideWindow(conn, length, msg);
        break;
    case GUI_MSG_REDRAW_WINDOW:
        handleRedrawWindow(conn, length, msg);
        break;
    case GUI_MSG_REDRAW_WINDOW_PART:
        handleRedrawWindowPart(conn, length, msg);
        break;
    case GUI_MSG_RESIZE_WINDOW:
        handleResizeWindow(conn, length, msg);
        break;
    case GUI_MSG_SET_WINDOW_BACKGROUND:
        handleSetWindowBackground(conn, length, msg);
        break;
    case GUI_MSG_SET_WINDOW_CURSOR:
        handleSetWindowCursor(conn, length, msg);
        break;
    case GUI_MSG_SET_WINDOW_TITLE:
        handleSetWindowTitle(conn, length, msg);
        break;
    case GUI_MSG_SHOW_WINDOW:
        handleShowWindow(conn, length, msg);
        break;
    }
}

bool receiveMessage(struct Connection* conn) {
    while (conn->headerReceived < sizeof(struct gui_msg_header)) {
        ssize_t bytesRead = read(conn->fd,
                (char*) &conn->headerBuffer + conn->headerReceived,
                sizeof(struct gui_msg_header) - conn->headerReceived);
        if (bytesRead < 0) {
            return errno == EAGAIN || errno == EWOULDBLOCK;
        }
        conn->headerReceived += bytesRead;
    }

    size_t length = conn->headerBuffer.length;

    if (!conn->messageBuffer) {
        conn->messageBuffer = malloc(length);
        if (!conn->messageBuffer) dxui_panic(context, "malloc");
    }

    while (conn->messageReceived < length) {
        ssize_t bytesRead = read(conn->fd,
                conn->messageBuffer + conn->messageReceived,
                length - conn->messageReceived);
        if (bytesRead < 0) {
            return errno == EAGAIN || errno == EWOULDBLOCK;
        }
        conn->messageReceived += bytesRead;
    }

    handleMessage(conn, conn->headerBuffer.type, length, conn->messageBuffer);
    free(conn->messageBuffer);
    conn->messageBuffer = NULL;
    conn->messageReceived = 0;
    conn->headerReceived = 0;

    return true;
}

void sendEvent(struct Connection* conn, unsigned int type, size_t length,
        void* msg) {
    struct gui_msg_header header;
    header.type = type;
    header.length = length;
    sendOutput(conn, &header, sizeof(header));
    sendOutput(conn, msg, length);
}

static bool sendOutput(struct Connection* conn, const void* buffer,
        size_t size) {
    const char* buf = buffer;
    if (!conn->outputBuffered) {
        while (size) {
            ssize_t written = write(conn->fd, buf, size);
            if (written < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                return false;
            }
            buf += written;
            size -= written;
        }
    }

    if (size) {
        if (size < conn->outputBufferSize - conn->outputBuffered) {
            size_t offset = (conn->outputBufferOffset + conn->outputBuffered) %
                    conn->outputBufferSize;
            size_t copySize = conn->outputBufferSize - offset;
            if (size < copySize) copySize = size;
            memcpy(conn->outputBuffer + offset, buf, copySize);
            buf += copySize;

            if (size - copySize) {
                memcpy(conn->outputBuffer, buf, size - copySize);
            }

            conn->outputBuffered += size;
        } else {
            char* newBuffer = malloc(conn->outputBuffered + size);
            if (!newBuffer) dxui_panic(context, "malloc");
            size_t copySize = conn->outputBufferSize - conn->outputBufferOffset;
            if (conn->outputBuffered < copySize) {
                copySize = conn->outputBuffered;
            }
            memcpy(newBuffer, conn->outputBuffer + conn->outputBufferOffset,
                    copySize);
            if (conn->outputBuffered > copySize) {
                memcpy(newBuffer + copySize, conn->outputBuffer,
                        conn->outputBuffered - copySize);
            }
            memcpy(newBuffer + conn->outputBuffered, buf, size);

            free(conn->outputBuffer);
            conn->outputBuffered += size;
            conn->outputBufferOffset = 0;
            conn->outputBufferSize = conn->outputBuffered;
        }
    }

    return true;
}

static void handleCloseWindow(struct Connection* conn, size_t length,
        struct gui_msg_close_window* msg) {
    if (length < sizeof(*msg)) return;
    struct Window* window = getWindow(conn, msg->window_id);
    if (!window) return;
    closeWindow(window);
}

static void handleCreateWindow(struct Connection* conn, size_t length,
        struct gui_msg_create_window* msg) {
    if (length < sizeof(*msg)) return;
    char* title = strndup(msg->title, length - sizeof(*msg));
    if (!title) dxui_panic(context, "malloc");
    struct Window* window = addWindow(msg->x, msg->y, msg->width, msg->height,
            title, msg->flags, conn);
    free(title);

    struct gui_event_window_created response;

    for (size_t i = 0; i < conn->windowsAllocated; i++) {
        if (!conn->windows[i]) {
            conn->windows[i] = window;
            window->id = i;
            response.window_id = i;
            sendEvent(conn, GUI_EVENT_WINDOW_CREATED, sizeof(response),
                    &response);
            return;
        }
    }

    size_t newSize = conn->windowsAllocated ? conn->windowsAllocated * 2 : 8;
    conn->windows = reallocarray(conn->windows, newSize,
            sizeof(struct Window*));
    if (!conn->windows) dxui_panic(context, "realloc");
    memset(conn->windows + conn->windowsAllocated, 0,
            (newSize - conn->windowsAllocated) * sizeof(struct Window*));

    conn->windows[conn->windowsAllocated] = window;
    window->id = conn->windowsAllocated;
    response.window_id = conn->windowsAllocated;
    sendEvent(conn, GUI_EVENT_WINDOW_CREATED, sizeof(response), &response);
    conn->windowsAllocated = newSize;
}

static void handleHideWindow(struct Connection* conn, size_t length,
        struct gui_msg_hide_window* msg) {
    if (length < sizeof(*msg)) return;
    struct Window* window = getWindow(conn, msg->window_id);
    if (!window) return;
    hideWindow(window);
}

static void handleRedrawWindow(struct Connection* conn, size_t length,
        struct gui_msg_redraw_window* msg) {
    if (length < sizeof(*msg)) return;
    size_t lfbSize = msg->height * msg->width * sizeof(dxui_color);
    if (length < sizeof(*msg) + lfbSize) return;
    struct Window* window = getWindow(conn, msg->window_id);
    if (!window) return;
    redrawWindow(window, msg->width, msg->height, msg->lfb);
}

static void handleRedrawWindowPart(struct Connection* conn, size_t length,
        struct gui_msg_redraw_window_part* msg) {
    if (length < sizeof(*msg)) return;
    size_t lfbSize = ((msg->height - 1) * msg->pitch + msg->width) *
            sizeof(dxui_color);
    if (length < sizeof(*msg) + lfbSize) return;
    struct Window* window = getWindow(conn, msg->window_id);
    if (!window) return;
    redrawWindowPart(window, msg->x, msg->y, msg->width, msg->height,
            msg->pitch, msg->lfb);
}

static void handleResizeWindow(struct Connection* conn, size_t length,
        struct gui_msg_resize_window* msg) {
    if (length < sizeof(*msg)) return;
    struct Window* window = getWindow(conn, msg->window_id);
    if (!window) return;
    dxui_rect rect = window->rect;
    rect.width = msg->width;
    rect.height = msg->height;
    resizeWindow(window, rect);
}

static void handleSetWindowBackground(struct Connection* conn, size_t length,
        struct gui_msg_set_window_background* msg) {
    if (length < sizeof(*msg)) return;
    struct Window* window = getWindow(conn, msg->window_id);
    if (!window) return;
    setWindowBackground(window, msg->color);
}

static void handleSetWindowCursor(struct Connection* conn, size_t length,
        struct gui_msg_set_window_cursor* msg) {
    if (length < sizeof(*msg)) return;
    struct Window* window = getWindow(conn, msg->window_id);
    if (!window) return;
    if (msg->cursor >= GUI_NUM_CURSORS) return;
    setWindowCursor(window, msg->cursor);
}

static void handleSetWindowTitle(struct Connection* conn, size_t length,
        struct gui_msg_set_window_title* msg) {
    if (length < sizeof(*msg)) return;
    struct Window* window = getWindow(conn, msg->window_id);
    if (!window) return;
    char* title = strndup(msg->title, length - sizeof(*msg));
    if (!title) dxui_panic(context, "malloc");
    setWindowTitle(window, title);
    free(title);
}

static void handleShowWindow(struct Connection* conn, size_t length,
        struct gui_msg_show_window* msg) {
    if (length < sizeof(*msg)) return;
    struct Window* window = getWindow(conn, msg->window_id);
    if (!window) return;
    showWindow(window);
}
