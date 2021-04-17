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

/* libdxui/src/events.c
 * Event handling.
 */

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/guimsg.h>
#include "context.h"

static Window* getWindow(dxui_context* context, unsigned int id);
static void handleCloseButton(dxui_context* context, size_t length,
        struct gui_event_window_close_button* msg);
static void handleKey(dxui_context* context, size_t length,
        struct gui_event_key* msg);
static void handleMessage(dxui_context* context, unsigned int type,
        size_t length, void* msg);
static void handleMouse(dxui_context* context, size_t length,
        struct gui_event_mouse* msg);
static void handleStatus(dxui_context* context, size_t length,
        struct gui_event_status* msg);
static void handleWindowCreated(dxui_context* context, size_t length,
        struct gui_event_window_created* msg);
static void handleWindowResized(dxui_context* context, size_t length,
        struct gui_event_window_resized* msg);
static bool receiveMessage(dxui_context* context);

bool dxui_pump_events(dxui_context* context, int mode) {
    struct pollfd pfd[1];
    pfd[0].fd = context->socket;
    pfd[0].events = POLLIN;

    while (true) {
        int result = poll(pfd, 1, mode == DXUI_PUMP_CLEAR ? 0 : -1);
        if (result < 0) {
            if (errno != EAGAIN && errno != EINTR) return false;
        } else if (result == 0) {
            if (mode == DXUI_PUMP_CLEAR) return true;
        } else {
            if (pfd[0].revents & POLLIN) {
                if (!receiveMessage(context)) {
                    return false;
                }
            }

            if (pfd[0].revents & POLLHUP || pfd[0].revents & POLLERR) {
                return false;
            }

            if (mode == DXUI_PUMP_ONCE) return true;
            if (mode == DXUI_PUMP_ONCE_CLEAR) {
                mode = DXUI_PUMP_CLEAR;
            }
        }

        if (mode == DXUI_PUMP_WHILE_WINDOWS_EXIST &&
                context->firstWindow == NULL) {
            return true;
        }
    }
}

void (dxui_set_event_handler)(dxui_control* control, int event, void* handler) {
    if (event < 0 || event >= DXUI_EVENT_NUM) return;
    control->internal->eventHandlers[event] = handler;
}

static Window* getWindow(dxui_context* context, unsigned int id) {
    for (Window* win = context->firstWindow; win; win = win->next) {
        if (win->idAssigned && win->id == id) {
            return win;
        }
    }

    return NULL;
}

static void handleCloseButton(dxui_context* context, size_t length,
        struct gui_event_window_close_button* msg) {
    if (length < sizeof(*msg)) return;

    Window* window = getWindow(context, msg->window_id);
    if (!window) return;

    void (*handler)(dxui_window*) =
            window->control.eventHandlers[DXUI_EVENT_WINDOW_CLOSE_BUTTON];
    if (handler) {
        handler(DXUI_AS_WINDOW(window));
    } else {
        dxui_close(DXUI_AS_WINDOW(window));
    }
}

static void handleKey(dxui_context* context, size_t length,
        struct gui_event_key* msg) {
    if (length < sizeof(*msg)) return;

    Window* window = getWindow(context, msg->window_id);
    if (!window) return;

    void (*handler)(dxui_window*, dxui_key_event*) =
            window->control.eventHandlers[DXUI_EVENT_KEY];
    if (handler) {
        dxui_key_event event;
        event.key = msg->key;
        event.codepoint = msg->codepoint;
        handler(DXUI_AS_WINDOW(window), &event);
    }
}

static void handleMessage(dxui_context* context, unsigned int type,
        size_t length, void* msg) {
    switch (type) {
    case GUI_EVENT_CLOSE_BUTTON:
        handleCloseButton(context, length, msg);
        break;
    case GUI_EVENT_KEY:
        handleKey(context, length, msg);
        break;
    case GUI_EVENT_MOUSE:
        handleMouse(context, length, msg);
        break;
    case GUI_EVENT_STATUS:
        handleStatus(context, length, msg);
        break;
    case GUI_EVENT_WINDOW_CREATED:
        handleWindowCreated(context, length, msg);
        break;
    case GUI_EVENT_WINDOW_RESIZED:
        handleWindowResized(context, length, msg);
        break;
    }
}

static void handleMouse(dxui_context* context, size_t length,
        struct gui_event_mouse* msg) {
    if (length < sizeof(*msg)) return;

    dxui_pos pos;
    pos.x = msg->x;
    pos.y = msg->y;

    bool mouseDown = false;
    bool mouseUp = false;

    if (!context->mouseDown && msg->flags & GUI_MOUSE_LEFT) {
        context->mouseDown = true;
        context->mouseDownPos = pos;
        mouseDown = true;
    } else if (context->mouseDown && !(msg->flags & GUI_MOUSE_LEFT)) {
        context->mouseDown = false;
        mouseUp = true;
    }

    Window* window = getWindow(context, msg->window_id);
    if (!window) return;

    bool click = false;

    dxui_control* control = dxui_get_control_at(window, pos);
    if (mouseUp) {
        dxui_control* mouseDownControl = dxui_get_control_at(window,
                context->mouseDownPos);
        if (control == mouseDownControl) {
            click = true;
        }
    }

    dxui_mouse_event event;
    event.pos = pos;
    event.flags = msg->flags;

    if (mouseDown) {
        void (*handler)(dxui_control*, dxui_mouse_event*) =
                control->internal->eventHandlers[DXUI_EVENT_MOUSE_DOWN];
        if (handler) {
            handler(control, &event);
        } else {
            mouseDown = false;
        }
    } else if (mouseUp) {
        void (*handler)(dxui_control*, dxui_mouse_event*) =
                control->internal->eventHandlers[DXUI_EVENT_MOUSE_UP];
        if (handler) {
            handler(control, &event);
        } else {
            mouseUp = false;
        }
    }
    if (click) {
        void (*handler)(dxui_control*, dxui_mouse_event*) =
                control->internal->eventHandlers[DXUI_EVENT_MOUSE_CLICK];
        if (handler) {
            handler(control, &event);
        } else {
            click = false;
        }
    }

    if (!mouseDown && !mouseUp && !click) {
        void (*handler)(dxui_control*, dxui_mouse_event*) =
                control->internal->eventHandlers[DXUI_EVENT_MOUSE];
        if (handler) {
            handler(control, &event);
        }
    }
}

static void handleStatus(dxui_context* context, size_t length,
        struct gui_event_status* msg) {
    if (length < sizeof(*msg)) return;

    context->displayDim.width = msg->display_width;
    context->displayDim.height = msg->display_height;
}

static void handleWindowCreated(dxui_context* context, size_t length,
        struct gui_event_window_created* msg) {
    if (length < sizeof(*msg)) return;

    for (Window* win = context->firstWindow; win; win = win->next) {
        if (!win->idAssigned) {
            win->id = msg->window_id;
            win->idAssigned = true;
            return;
        }
    }
}

static void handleWindowResized(dxui_context* context, size_t length,
        struct gui_event_window_resized* msg) {
    if (length < sizeof(*msg)) return;

    Window* window = getWindow(context, msg->window_id);
    if (!window) return;

    window->control.rect.width = msg->width;
    window->control.rect.height = msg->height;

    void (*handler)(dxui_window*, dxui_resize_event*) =
            window->control.eventHandlers[DXUI_EVENT_WINDOW_RESIZED];
    if (handler) {
        dxui_resize_event event;
        event.dim = window->control.rect.dim;
        handler(DXUI_AS_WINDOW(window), &event);
    }

    dxui_dim dim = window->control.rect.dim;
    dxui_color* lfb = malloc(dim.width * dim.height * sizeof(uint32_t));
    if (!lfb) return;
    free(window->lfb);
    window->lfb = lfb;
    window->lfbDim = dim;
    window->redraw = true;
    dxui_update(window);
}

static bool receiveMessage(dxui_context* context) {
    struct gui_msg_header headerBuffer;
    size_t headerReceived = 0;

    while (headerReceived < sizeof(struct gui_msg_header)) {
        ssize_t bytesRead = read(context->socket,
                (char*) &headerBuffer + headerReceived,
                sizeof(struct gui_msg_header) - headerReceived);
        if (bytesRead < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        headerReceived += bytesRead;
    }

    size_t length = headerBuffer.length;

    char* messageBuffer = malloc(length);
    if (!messageBuffer) return false;
    size_t messageReceived = 0;

    while (messageReceived < length) {
        ssize_t bytesRead = read(context->socket,
                messageBuffer + messageReceived, length - messageReceived);
        if (bytesRead < 0) {
            if (errno == EINTR) continue;
            free(messageBuffer);
            return false;
        }
        messageReceived += bytesRead;
    }

    handleMessage(context, headerBuffer.type, length, messageBuffer);
    free(messageBuffer);

    return true;
}
