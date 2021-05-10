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

/* libdxui/src/window.c
 * Windows.
 */

#include <dxui.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/guimsg.h>
#include "context.h"

static void deleteWindow(Control* control);
static void redrawWindow(Control* control, dxui_dim dim, dxui_color* lfb,
        unsigned int pitch);
static dxui_context* getWindowContext(Container* container);
static dxui_color* getWindowFramebuffer(Container* container, dxui_dim* dim,
        unsigned int* pitch);
static void invalidateWindowRect(Container* container, dxui_rect rect);
static bool writeAll(int fd, const void* buffer, size_t size);

const ControlClass dxui_windowControlClass = {
    .delete = deleteWindow,
    .redraw = redrawWindow,
};
static const ContainerClass windowContainerClass = {
    .getContext = getWindowContext,
    .getFramebuffer = getWindowFramebuffer,
    .invalidate = invalidateWindowRect,
};

void dxui_close(dxui_window* window) {
    Window* win = window->internal;
    dxui_context* context = win->context;

    struct gui_msg_close_window msg;
    msg.window_id = win->id;
    struct gui_msg_header header;
    header.type = GUI_MSG_CLOSE_WINDOW;
    header.length = sizeof(msg);

    writeAll(context->socket, &header, sizeof(header));
    writeAll(context->socket, &msg, sizeof(msg));

    void (*handler)(dxui_window*) =
            win->control.eventHandlers[DXUI_EVENT_WINDOW_CLOSE];
    if (handler) {
        handler(window);
    }

    if (win->prev) {
        win->prev->next = win->next;
    } else {
        context->firstWindow = win->next;
    }

    if (win->next) {
        win->next->prev = win->prev;
    }

    Control* control = win->container.firstControl;
    while (control) {
        Control* next = control->next;
        dxui_delete(control);
        control = next;
    }

    free(win->lfb);
    free(window);
}

dxui_window* dxui_create_window(dxui_context* context, dxui_rect rect,
        const char* title, int flags) {
    Window* window = calloc(1, sizeof(Window));
    if (!window) return NULL;
    window->control.self = window;
    window->control.class = &dxui_windowControlClass;
    window->control.text = strdup(title);
    if (!window->control.text) {
        free(window);
        return NULL;
    }
    window->compositorTitle = window->control.text;
    window->control.rect = rect;
    window->control.background = COLOR_WHITE_SMOKE;
    window->compositorBackground = COLOR_WHITE_SMOKE;
    window->container.class = &windowContainerClass;
    window->context = context;
    window->idAssigned = false;

    window->lfbDim = rect.dim;
    window->lfb = malloc(rect.width * rect.height * sizeof(uint32_t));
    if (!window->lfb) {
        free(window->control.text);
        free(window);
        return NULL;
    }
    window->redraw = true;

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

    if (!(writeAll(context->socket, &header, sizeof(header)) &&
            writeAll(context->socket, &msg, sizeof(msg)) &&
            writeAll(context->socket, title, strlen(title)))) {
        free(window->lfb);
        free(window->control.text);
        free(window);
        return NULL;
    }

    window->next = context->firstWindow;
    if (window->next) {
        window->next->prev = window;
    }
    context->firstWindow = window;

    while (!window->idAssigned) {
        dxui_pump_events(context, DXUI_PUMP_ONCE);
    }

    dxui_update(window);
    return DXUI_AS_WINDOW(window);
}

void dxui_hide(dxui_window* window) {
    Window* win = window->internal;
    win->visible = false;

    struct gui_msg_hide_window msg;
    msg.window_id = win->id;
    struct gui_msg_header header;
    header.type = GUI_MSG_HIDE_WINDOW;
    header.length = sizeof(msg);
    writeAll(win->context->socket, &header, sizeof(header));
    writeAll(win->context->socket, &msg, sizeof(msg));
}

void dxui_resize_window(dxui_window* window, dxui_dim dim) {
    Window* win = window->internal;
    win->control.rect.dim = dim;

    struct gui_msg_resize_window msg;
    msg.window_id = win->id;
    msg.width = dim.width;
    msg.height = dim.height;
    struct gui_msg_header header;
    header.type = GUI_MSG_RESIZE_WINDOW;
    header.length = sizeof(msg);
    writeAll(win->context->socket, &header, sizeof(header));
    writeAll(win->context->socket, &msg, sizeof(msg));
}

void dxui_set_cursor(dxui_window* window, int cursor) {
    Window* win = window->internal;

    struct gui_msg_set_window_cursor msg;
    msg.window_id = win->id;
    msg.cursor = cursor;
    struct gui_msg_header header;
    header.type = GUI_MSG_SET_WINDOW_CURSOR;
    header.length = sizeof(msg);
    writeAll(win->context->socket, &header, sizeof(header));
    writeAll(win->context->socket, &msg, sizeof(msg));
}

void dxui_show(dxui_window* window) {
    Window* win = window->internal;
    win->visible = true;

    struct gui_msg_show_window msg;
    msg.window_id = win->id;
    struct gui_msg_header header;
    header.type = GUI_MSG_SHOW_WINDOW;
    header.length = sizeof(msg);
    writeAll(win->context->socket, &header, sizeof(header));
    writeAll(win->context->socket, &msg, sizeof(msg));
}

static void deleteWindow(Control* control) {
    Window* window = (Window*) control;
    dxui_close(DXUI_AS_WINDOW(window));
}

static void redrawWindow(Control* control, dxui_dim dim, dxui_color* lfb,
        unsigned int pitch) {
    Window* window = (Window*) control;
    window->updateInProgress = true;

    // Inform the compositor about changed backgrounds and titles.
    if (window->compositorBackground != control->background) {
        struct gui_msg_set_window_background msg;
        msg.window_id = window->id;
        msg.color = control->background;
        struct gui_msg_header header;
        header.type = GUI_MSG_SET_WINDOW_BACKGROUND;
        header.length = sizeof(msg);
        writeAll(window->context->socket, &header, sizeof(header));
        writeAll(window->context->socket, &msg, sizeof(msg));
        window->compositorBackground = control->background;
    }

    if (window->compositorTitle != control->text) {
        size_t titleLength = strlen(control->text);
        struct gui_msg_set_window_title msg;
        msg.window_id = window->id;
        struct gui_msg_header header;
        header.type = GUI_MSG_SET_WINDOW_TITLE;
        header.length = sizeof(msg) + titleLength;
        writeAll(window->context->socket, &header, sizeof(header));
        writeAll(window->context->socket, &msg, sizeof(msg));
        writeAll(window->context->socket, control->text, titleLength);
        window->compositorTitle = control->text;
    }

    for (int y = 0; y < dim.height; y++) {
        for (int x = 0; x < dim.width; x++) {
            lfb[y * pitch + x] = control->background;
        }
    }

    control = window->container.firstControl;
    while (control) {
        control->class->redraw(control, dim, lfb, pitch);
        control = control->next;
    }
    window->updateInProgress = false;

    struct gui_msg_redraw_window msg;
    msg.window_id = window->id;
    msg.width = window->lfbDim.width;
    msg.height = window->lfbDim.height;
    struct gui_msg_header header;
    header.type = GUI_MSG_REDRAW_WINDOW;
    size_t lfbSize = window->lfbDim.width * window->lfbDim.height *
            sizeof(uint32_t);
    header.length = sizeof(msg) + lfbSize;
    writeAll(window->context->socket, &header, sizeof(header));
    writeAll(window->context->socket, &msg, sizeof(msg));
    writeAll(window->context->socket, window->lfb, lfbSize);

    window->redraw = false;
}

static dxui_context* getWindowContext(Container* container) {
    Window* window = (Window*) container;
    return window->context;
}

static dxui_color* getWindowFramebuffer(Container* container, dxui_dim* dim,
        unsigned int* pitch) {
    Window* window = (Window*) container;
    *dim = window->lfbDim;
    *pitch = window->lfbDim.width;
    return window->lfb;
}

static void updateRect(Window* window, dxui_rect rect) {
    struct gui_msg_redraw_window_part msg;
    msg.window_id = window->id;
    msg.pitch = window->lfbDim.width;
    msg.x = rect.x;
    msg.y = rect.y;
    msg.width = rect.width;
    msg.height = rect.height;
    struct gui_msg_header header;
    header.type = GUI_MSG_REDRAW_WINDOW_PART;
    size_t lfbSize = ((rect.height - 1) * window->lfbDim.width + rect.width) *
            sizeof(uint32_t);
    header.length = sizeof(msg) + lfbSize;
    writeAll(window->context->socket, &header, sizeof(header));
    writeAll(window->context->socket, &msg, sizeof(msg));
    writeAll(window->context->socket, window->lfb + rect.y *
            window->lfbDim.width + rect.x, lfbSize);
}

static void invalidateWindowRect(Container* container, dxui_rect rect) {
    Window* window = (Window*) container;
    if (window->updateInProgress) return;
    updateRect(window, dxui_rect_crop(rect, window->lfbDim));
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
}
