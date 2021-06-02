/* Copyright (c) 2021 Dennis Wölfing
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

/* libdxui/src/compositor.c
 * Compositor backend.
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/guimsg.h>
#include "context.h"

static void closeWindow(dxui_context* context, unsigned int id);
static void createWindow(dxui_context* context, dxui_rect rect,
        const char* title, int flags);
static void hideWindow(dxui_context* context, unsigned int id);
static void resizeWindow(dxui_context* context, unsigned int id, dxui_dim dim);
static void setWindowCursor(dxui_context* context, unsigned int id, int cursor);
static void showWindow(dxui_context* context, unsigned int id);
static void setWindowBackground(dxui_context* context, unsigned int id,
        dxui_color color);
static void setWindowTitle(dxui_context* context, unsigned int id,
        const char* title);
static void redrawWindow(dxui_context* context, unsigned int id, dxui_dim dim,
        dxui_color* lfb);
static void redrawWindowPart(dxui_context* context, unsigned int id,
        unsigned int pitch, dxui_rect rect, dxui_color* lfb);
static bool writeAll(int fd, const void* buffer, size_t size);

const Backend dxui_compositorBackend = {
    .closeWindow = closeWindow,
    .createWindow = createWindow,
    .hideWindow = hideWindow,
    .resizeWindow = resizeWindow,
    .setWindowCursor = setWindowCursor,
    .showWindow = showWindow,
    .setWindowBackground = setWindowBackground,
    .setWindowTitle = setWindowTitle,
    .redrawWindow = redrawWindow,
    .redrawWindowPart = redrawWindowPart,
};

static void closeWindow(dxui_context* context, unsigned int id) {
    struct gui_msg_close_window msg;
    msg.window_id = id;
    struct gui_msg_header header;
    header.type = GUI_MSG_CLOSE_WINDOW;
    header.length = sizeof(msg);

    writeAll(context->socket, &header, sizeof(header));
    writeAll(context->socket, &msg, sizeof(msg));
}

static void createWindow(dxui_context* context, dxui_rect rect,
        const char* title, int flags) {
    struct gui_msg_create_window msg;
    msg.x = rect.x;
    msg.y = rect.y;
    msg.width = rect.width;
    msg.height = rect.height;
    msg.flags = 0;
    if (flags & DXUI_WINDOW_NO_RESIZE) msg.flags |= GUI_WINDOW_NO_RESIZE;

    struct gui_msg_header header;
    header.type = GUI_MSG_CREATE_WINDOW;
    header.length = sizeof(msg) + strlen(title);

    writeAll(context->socket, &header, sizeof(header));
    writeAll(context->socket, &msg, sizeof(msg));
    writeAll(context->socket, title, strlen(title));
}

static void hideWindow(dxui_context* context, unsigned int id) {
    struct gui_msg_hide_window msg;
    msg.window_id = id;
    struct gui_msg_header header;
    header.type = GUI_MSG_HIDE_WINDOW;
    header.length = sizeof(msg);
    writeAll(context->socket, &header, sizeof(header));
    writeAll(context->socket, &msg, sizeof(msg));
}

static void resizeWindow(dxui_context* context, unsigned int id, dxui_dim dim) {
    struct gui_msg_resize_window msg;
    msg.window_id = id;
    msg.width = dim.width;
    msg.height = dim.height;
    struct gui_msg_header header;
    header.type = GUI_MSG_RESIZE_WINDOW;
    header.length = sizeof(msg);
    writeAll(context->socket, &header, sizeof(header));
    writeAll(context->socket, &msg, sizeof(msg));
}

static void setWindowCursor(dxui_context* context, unsigned int id,
        int cursor) {
    struct gui_msg_set_window_cursor msg;
    msg.window_id = id;
    msg.cursor = cursor;
    struct gui_msg_header header;
    header.type = GUI_MSG_SET_WINDOW_CURSOR;
    header.length = sizeof(msg);
    writeAll(context->socket, &header, sizeof(header));
    writeAll(context->socket, &msg, sizeof(msg));
}

static void showWindow(dxui_context* context, unsigned int id) {
    struct gui_msg_show_window msg;
    msg.window_id = id;
    struct gui_msg_header header;
    header.type = GUI_MSG_SHOW_WINDOW;
    header.length = sizeof(msg);
    writeAll(context->socket, &header, sizeof(header));
    writeAll(context->socket, &msg, sizeof(msg));
}

static void setWindowBackground(dxui_context* context, unsigned int id,
        dxui_color color) {
    struct gui_msg_set_window_background msg;
    msg.window_id = id;
    msg.color = color;
    struct gui_msg_header header;
    header.type = GUI_MSG_SET_WINDOW_BACKGROUND;
    header.length = sizeof(msg);
    writeAll(context->socket, &header, sizeof(header));
    writeAll(context->socket, &msg, sizeof(msg));
}

static void setWindowTitle(dxui_context* context, unsigned int id,
        const char* title) {
    size_t titleLength = strlen(title);
    struct gui_msg_set_window_title msg;
    msg.window_id = id;
    struct gui_msg_header header;
    header.type = GUI_MSG_SET_WINDOW_TITLE;
    header.length = sizeof(msg) + titleLength;
    writeAll(context->socket, &header, sizeof(header));
    writeAll(context->socket, &msg, sizeof(msg));
    writeAll(context->socket, title, titleLength);
}

static void redrawWindow(dxui_context* context, unsigned int id, dxui_dim dim,
        dxui_color* lfb) {
    struct gui_msg_redraw_window msg;
    msg.window_id = id;
    msg.width = dim.width;
    msg.height = dim.height;
    struct gui_msg_header header;
    header.type = GUI_MSG_REDRAW_WINDOW;
    size_t lfbSize = dim.width * dim.height * sizeof(uint32_t);
    header.length = sizeof(msg) + lfbSize;
    writeAll(context->socket, &header, sizeof(header));
    writeAll(context->socket, &msg, sizeof(msg));
    writeAll(context->socket, lfb, lfbSize);
}

static void redrawWindowPart(dxui_context* context, unsigned int id,
        unsigned int pitch, dxui_rect rect, dxui_color* lfb) {
    if (rect.width == 0 || rect.height == 0) return;

    struct gui_msg_redraw_window_part msg;
    msg.window_id = id;
    msg.pitch = pitch;
    msg.x = rect.x;
    msg.y = rect.y;
    msg.width = rect.width;
    msg.height = rect.height;
    struct gui_msg_header header;
    header.type = GUI_MSG_REDRAW_WINDOW_PART;
    size_t lfbSize = ((rect.height - 1) * pitch + rect.width) *
            sizeof(uint32_t);
    header.length = sizeof(msg) + lfbSize;
    writeAll(context->socket, &header, sizeof(header));
    writeAll(context->socket, &msg, sizeof(msg));
    writeAll(context->socket, lfb + rect.y * pitch + rect.x, lfbSize);
}

static bool writeAll(int fd, const void* buffer, size_t size) {
    const char* buf = buffer;
    size_t written = 0;
    while (written < size) {
        ssize_t bytesWritten = write(fd, buf + written, size - written);
        if (bytesWritten < 0) {
            if (errno != EINTR) return false;
        } else {
            written += bytesWritten;
        }
    }
    return true;
};
