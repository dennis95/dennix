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
#include <dennix/mouse.h>
#include "context.h"

static Window* getWindow(dxui_context* context, unsigned int id);
static void handleMouseEvent(dxui_context* context, Window* window,
        dxui_mouse_event event);

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

static bool handleKeyboard(dxui_context* context);
static void handleMousePacket(dxui_context* context,
        const struct mouse_data* data);
static bool handleMousePackets(dxui_context* context);

bool dxui_pump_events(dxui_context* context, int mode, int timeout) {
    struct pollfd pfd[2];
    nfds_t nfds;
    if (context->socket != -1) {
        pfd[0].fd = context->socket;
        pfd[0].events = POLLIN;
        nfds = 1;
    } else {
        pfd[0].fd = 0;
        pfd[0].events = POLLIN;
        pfd[1].fd = context->mouseFd;
        pfd[1].events = POLLIN;
        nfds = 2;
    }

    while (true) {
        int result = poll(pfd, nfds, mode == DXUI_PUMP_CLEAR ? 0 : timeout);
        if (result < 0) {
            if (errno != EAGAIN && errno != EINTR) return false;
        } else if (result == 0) {
            return true;
        } else {
            if (context->socket != -1) {
                if (pfd[0].revents & POLLIN) {
                    if (!receiveMessage(context)) return false;
                }

                if (pfd[0].revents & POLLHUP || pfd[0].revents & POLLERR) {
                    return false;
                }
            } else {
                if (pfd[0].revents & POLLIN) {
                    if (!handleKeyboard(context)) return false;
                }

                if (pfd[1].revents & POLLIN) {
                    if (!handleMousePackets(context)) return false;
                }
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

static void handleMouseEvent(dxui_context* context, Window* window,
        dxui_mouse_event event) {
    bool mouseDown = false;
    bool mouseUp = false;

    if (!context->mouseDown && event.flags & DXUI_MOUSE_LEFT) {
        context->mouseDown = true;
        context->mouseDownPos = event.pos;
        mouseDown = true;
    } else if (context->mouseDown && !(event.flags & DXUI_MOUSE_LEFT)) {
        context->mouseDown = false;
        mouseUp = true;
    }

    bool click = false;

    dxui_control* control = dxui_get_control_at(window, event.pos);
    if (mouseUp) {
        dxui_control* mouseDownControl = dxui_get_control_at(window,
                context->mouseDownPos);
        if (control == mouseDownControl) {
            click = true;
        }
    }

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
    Window* window = getWindow(context, msg->window_id);
    if (!window) return;

    dxui_mouse_event event;
    event.pos.x = msg->x;
    event.pos.y = msg->y;
    event.flags = msg->flags;
    handleMouseEvent(context, window, event);
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

static bool handleKeyboard(dxui_context* context) {
    struct kbwc kbwc[1024];

    for (size_t i = 0; i < context->partialKeyBytes; i++) {
        ((char*) kbwc)[i] = context->partialKeyBuffer[i];
    }

    ssize_t bytes = read(0, (char*) kbwc + context->partialKeyBytes,
            sizeof(kbwc) - context->partialKeyBytes);
    if (bytes < 0) return false;
    bytes += context->partialKeyBytes;

    Window* window = context->activeWindow;

    size_t numKeys = bytes / sizeof(struct kbwc);
    for (size_t i = 0; i < numKeys; i++) {
        if (window) {
            void (*handler)(dxui_window*, dxui_key_event*) =
                    window->control.eventHandlers[DXUI_EVENT_KEY];
            if (handler) {
                dxui_key_event event;
                event.key = kbwc[i].kb;
                event.codepoint = kbwc[i].wc;
                handler(DXUI_AS_WINDOW(window), &event);
            }
        }
    }

    context->partialKeyBytes = bytes % sizeof(struct kbwc);
    for (size_t i = 0; i < context->partialKeyBytes; i++) {
        context->partialKeyBuffer[i] = *(char*) kbwc + bytes -
                context->partialKeyBytes + i;
    }
    return true;
}

static void handleMousePacket(dxui_context* context,
        const struct mouse_data* data) {
    context->mousePos.x += data->mouse_x;
    context->mousePos.y += data->mouse_y;
    if (context->mousePos.x < 0) {
        context->mousePos.x = 0;
    } else if (context->mousePos.x >= context->displayDim.width) {
        context->mousePos.x = context->displayDim.width - 1;
    }

    if (context->mousePos.y < 0) {
        context->mousePos.y = 0;
    } else if (context->mousePos.y >= context->displayDim.height) {
        context->mousePos.y = context->displayDim.height - 1;
    }

    if (!context->activeWindow) return;

    dxui_mouse_event event;
    event.pos.x = context->mousePos.x - context->viewport.x;
    event.pos.y = context->mousePos.y - context->viewport.y;
    event.flags = 0;

    if (data->mouse_flags & MOUSE_LEFT) {
        event.flags |= DXUI_MOUSE_LEFT;
    }
    if (data->mouse_flags & MOUSE_RIGHT) {
        event.flags |= DXUI_MOUSE_RIGHT;
    }
    if (data->mouse_flags & MOUSE_MIDDLE) {
        event.flags |= DXUI_MOUSE_MIDDLE;
    }
    if (data->mouse_flags & MOUSE_SCROLL_UP) {
        event.flags |= DXUI_MOUSE_SCROLL_UP;
    }
    if (data->mouse_flags & MOUSE_SCROLL_DOWN) {
        event.flags |= DXUI_MOUSE_SCROLL_DOWN;
    }

    handleMouseEvent(context, context->activeWindow, event);

    if (context->activeWindow) {
        dxui_update(context->activeWindow);
    }
}

static bool handleMousePackets(dxui_context* context) {
    struct mouse_data data[256];
    ssize_t bytesRead = read(context->mouseFd, &data, sizeof(data));
    if (bytesRead < 0) return false;
    size_t mousePackets = bytesRead / sizeof(struct mouse_data);

    for (size_t i = 0; i < mousePackets; i++) {
        handleMousePacket(context, &data[i]);
    }
    return true;
}
