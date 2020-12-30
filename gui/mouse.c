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

/* gui/mouse.c
 * Mouse input.
 */

#include <err.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <dennix/mouse.h>

#include "connection.h"
#include "display.h"
#include "gui.h"
#include "mouse.h"
#include "window.h"

int mouseFd;
int mouseX;
int mouseY;

static const int cursorSize = 48;

static uint32_t arrowCursor[48 * 48];
static uint32_t resizeD1Cursor[48 * 48];
static uint32_t resizeD2Cursor[48 * 48];
static uint32_t resizeHCursor[48 * 48];
static uint32_t resizeVCursor[48 * 48];

static uint32_t* cursor;
static bool leftClick;
static int resizeDirection;

static void handleMousePacket(const struct mouse_data* data);

struct Rectangle getMouseRect(void) {
    struct Rectangle result;
    result.x = mouseX - cursorSize / 2;
    result.y = mouseY - cursorSize / 2;
    result.width = cursorSize;
    result.height = cursorSize;
    return result;
}

static void handleMousePacket(const struct mouse_data* data) {
    int oldX = mouseX;
    int oldY = mouseY;

    mouseX += data->mouse_x;
    mouseY += data->mouse_y;
    if (mouseX < 0) {
        mouseX = 0;
    } else if (mouseX >= displayRect.width) {
        mouseX = displayRect.width - 1;
    }

    if (mouseY < 0) {
        mouseY = 0;
    } else if (mouseY >= displayRect.height) {
        mouseY = displayRect.height - 1;
    }

    int status = 0;
    struct Window* win = NULL;
    if (!(data->mouse_flags & MOUSE_LEFT) || !changingWindow) {
        for (win = topWindow; win; win = win->below) {
            if (!win->visible) continue;
            status = checkMouseInteraction(win, mouseX, mouseY);
            if (status) break;
        }
    }

    if (!leftClick) {
        uint32_t* newCursor;
        if (status == RESIZE_LEFT || status == RESIZE_RIGHT) {
            newCursor = resizeHCursor;
        } else if (status == RESIZE_TOP || status == RESIZE_BOTTOM) {
            newCursor = resizeVCursor;
        } else if (status == RESIZE_TOP_LEFT ||
                status == RESIZE_BOTTOM_RIGHT) {
            newCursor = resizeD1Cursor;
        } else if (status == RESIZE_TOP_RIGHT ||
                status == RESIZE_BOTTOM_LEFT) {
            newCursor = resizeD2Cursor;
        } else {
            newCursor = arrowCursor;
        }

        if (cursor != newCursor) {
            cursor = newCursor;
            addDamageRect(getMouseRect());
        }

        if (data->mouse_flags & MOUSE_LEFT) {
            leftClick = true;
        }

        if (leftClick && win) {
            moveWindowToTop(win);
            if (status == CLOSE_BUTTON) {
                struct gui_event_window_close_button msg;
                msg.window_id = win->id;
                sendEvent(win->connection, GUI_EVENT_CLOSE_BUTTON, sizeof(msg),
                        &msg);
            } else if (status == TITLE_BAR) {
                changingWindow = win;
            } else {
                changingWindow = win;
                resizeDirection = status;
            }
        }
    } else if (!(data->mouse_flags & MOUSE_LEFT)) {
        leftClick = false;
        changingWindow = NULL;
        resizeDirection = 0;
    } else if (changingWindow && resizeDirection == 0) {
        addDamageRect(changingWindow->rect);
        changingWindow->rect.x += mouseX - oldX;
        changingWindow->rect.y += mouseY - oldY;
        addDamageRect(changingWindow->rect);
    } else if (changingWindow) {
        struct Rectangle rect = changingWindow->rect;
        if (resizeDirection & RESIZE_LEFT) {
            rect.width += rect.x - mouseX;
            rect.x = mouseX;
        } else if (resizeDirection & RESIZE_RIGHT) {
            rect.width = mouseX - rect.x;
        }
        if (resizeDirection & RESIZE_TOP) {
            rect.height += rect.y - mouseY;
            rect.y = mouseY;
        } else if (resizeDirection & RESIZE_BOTTOM) {
            rect.height = mouseY - rect.y;
        }

        if (!rectEqual(rect, changingWindow->rect)) {
            resizeWindow(changingWindow, rect);
        }
    }

    if (win && status == CLIENT_AREA) {
        if (mouseWindow && mouseWindow != win) {
            struct gui_event_mouse event;
            event.window_id = mouseWindow->id;
            event.x = 0;
            event.y = 0;
            event.flags = GUI_MOUSE_LEAVE;
            sendEvent(mouseWindow->connection, GUI_EVENT_MOUSE, sizeof(event),
                    &event);
            mouseWindow = win;
        }

        struct gui_event_mouse event;
        event.window_id = win->id;
        event.x = mouseX - getClientRect(win).x;
        event.y = mouseY - getClientRect(win).y;
        event.flags = 0;
        if (leftClick) event.flags |= GUI_MOUSE_LEFT;
        if (data->mouse_flags & MOUSE_RIGHT) {
            event.flags |= GUI_MOUSE_RIGHT;
        }
        if (data->mouse_flags & MOUSE_MIDDLE) {
            event.flags |= GUI_MOUSE_MIDDLE;
        }
        if (data->mouse_flags & MOUSE_SCROLL_UP) {
            event.flags |= GUI_MOUSE_SCROLL_UP;
        }
        if (data->mouse_flags & MOUSE_SCROLL_DOWN) {
            event.flags |= GUI_MOUSE_SCROLL_DOWN;
        }
        sendEvent(win->connection, GUI_EVENT_MOUSE, sizeof(event), &event);
    } else if (mouseWindow) {
        struct gui_event_mouse event;
        event.window_id = mouseWindow->id;
        event.x = 0;
        event.y = 0;
        event.flags = GUI_MOUSE_LEAVE;
        sendEvent(mouseWindow->connection, GUI_EVENT_MOUSE, sizeof(event),
                &event);
        mouseWindow = NULL;
    }
}

void handleMouse(void) {
    struct Rectangle oldMouseRect = getMouseRect();

    struct mouse_data data[256];
    ssize_t bytesRead = read(mouseFd, &data, sizeof(data));
    if (bytesRead < 0) err(1, "read");
    size_t mousePackets = bytesRead / sizeof(struct mouse_data);

    for (size_t i = 0; i < mousePackets; i++) {
        handleMousePacket(&data[i]);
    }

    if (!rectEqual(oldMouseRect, getMouseRect())) {
        addDamageRect(oldMouseRect);
        addDamageRect(getMouseRect());
    }
}

void initializeMouse(void) {
    loadFromFile("/share/cursors/arrow.rgba", arrowCursor, sizeof(arrowCursor));
    loadFromFile("/share/cursors/resize_diagonal1.rgba", resizeD1Cursor,
            sizeof(resizeD1Cursor));
    loadFromFile("/share/cursors/resize_diagonal2.rgba", resizeD2Cursor,
            sizeof(resizeD2Cursor));
    loadFromFile("/share/cursors/resize_horizontal.rgba", resizeHCursor,
            sizeof(resizeHCursor));
    loadFromFile("/share/cursors/resize_vertical.rgba", resizeVCursor,
            sizeof(resizeVCursor));

    cursor = arrowCursor;

    mouseFd = open("/dev/mouse", O_RDONLY | O_CLOEXEC);
    if (mouseFd >= 0) {
        // Discard any mouse data that has been buffered.
        struct pollfd pfd[1];
        pfd[0].fd = mouseFd;
        pfd[0].events = POLLIN;
        while (poll(pfd, 1, 0) == 1) {
            struct mouse_data data[256];
            read(mouseFd, data, sizeof(data));
        }
    }
}

uint32_t renderCursor(int x, int y) {
    return cursor[y * cursorSize + x];
}
