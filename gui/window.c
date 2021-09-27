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

/* gui/window.c
 * Window.
 */

#include <stdlib.h>
#include <string.h>

#include "connection.h"
#include "window.h"

static const int windowBorderSize = 4;
static const int windowCloseButtonSize = 16;
static const int windowTitleBarSize = 16 + 2 * windowBorderSize;

static const dxui_color closeButtonColor = COLOR_RED;
static const dxui_color closeCrossColor = COLOR_WHITE;
static const dxui_color titleColor = COLOR_BLACK;
static const dxui_color windowDecorationColor = RGBA(64, 64, 180, 200);

struct Window* changingWindow;
struct Window* mouseWindow;
struct Window* topWindow;

static void addWindowOnTop(struct Window* window);
static dxui_rect chooseWindowRect(int x, int y, int width, int height);
static dxui_rect getCloseButtonRect(struct Window* window);
static void removeWindow(struct Window* window);
static dxui_color renderCloseButton(int x, int y);

static void addWindowOnTop(struct Window* window) {
    bool relativeMouse = false;

    if (topWindow) {
        topWindow->above = window;
        relativeMouse = topWindow->relativeMouse;
    }
    window->below = topWindow;
    window->above = NULL;
    topWindow = window;

    if (window->relativeMouse != relativeMouse) {
        dxui_set_relative_mouse(compositorWindow, window->relativeMouse);
    }

    if (window->visible) {
        addDamageRect(window->rect);
    }
}

struct Window* addWindow(int x, int y, int width, int height, const char* title,
        int flags, struct Connection* connection) {
    struct Window* window = malloc(sizeof(struct Window));
    if (!window) dxui_panic(context, "malloc");

    window->connection = connection;
    window->background = COLOR_WHITE_SMOKE;
    window->cursor = DXUI_CURSOR_ARROW;
    window->flags = flags;
    window->rect = chooseWindowRect(x, y, width, height);
    window->titleLfb = NULL;
    window->lfb = NULL;
    window->clientDim = (dxui_dim) {0, 0};
    window->relativeMouse = false;
    window->visible = false;

    setWindowTitle(window, title);
    addWindowOnTop(window);
    return window;
}

int checkMouseInteraction(struct Window* window, dxui_pos pos) {
    if (dxui_rect_contains_pos(window->rect, pos)) {
        if (dxui_rect_contains_pos(getClientRect(window), pos)) {
            return CLIENT_AREA;
        } else if (dxui_rect_contains_pos(getCloseButtonRect(window), pos)) {
            return CLOSE_BUTTON;
        } else {
            int result = 0;
            if (!(window->flags & GUI_WINDOW_NO_RESIZE)) {
                if (pos.x - window->rect.x < windowBorderSize) {
                    result |= RESIZE_LEFT;
                }
                if (pos.x - window->rect.x >= window->rect.width -
                        windowBorderSize) {
                    result |= RESIZE_RIGHT;
                }
                if (pos.y - window->rect.y < windowBorderSize) {
                    result |= RESIZE_TOP;
                }
                if (pos.y - window->rect.y >= window->rect.height -
                        windowBorderSize) {
                    result |= RESIZE_BOTTOM;
                }
            }

            if (!result) {
                return TITLE_BAR;
            }

            return result;
        }
    }

    return 0;
}

static dxui_rect chooseWindowRect(int x, int y, int width, int height) {
    dxui_rect rect;
    rect.x = x;
    rect.y = y;
    rect.width = width + 2 * windowBorderSize;
    rect.height = height + windowTitleBarSize + windowBorderSize;

    if (x < 0) {
        int maxX = guiDim.width - rect.width;
        if (maxX < 0) maxX = 50;
        rect.x = arc4random_uniform(maxX + 1);
    }

    if (y < 0) {
        int maxY = guiDim.height - rect.height;
        if (maxY < 0) maxY = 50;
        rect.y = arc4random_uniform(maxY + 1);
    }

    return rect;
}

void closeWindow(struct Window* window) {
    if (changingWindow == window) {
        changingWindow = NULL;
    }
    if (mouseWindow == window) {
        mouseWindow = NULL;
    }

    removeWindow(window);
    if (window->visible) {
        addDamageRect(window->rect);
    }
    window->connection->windows[window->id] = NULL;
    free(window->titleLfb);
    free(window->lfb);
    free(window);
}

dxui_rect getClientRect(struct Window* window) {
    dxui_rect result;
    result.x = window->rect.x + windowBorderSize;
    result.y = window->rect.y + windowTitleBarSize;
    result.width = window->rect.width - 2 * windowBorderSize;
    result.height = window->rect.height - windowTitleBarSize - windowBorderSize;
    return result;
}

static dxui_rect getCloseButtonRect(struct Window* window) {
    dxui_rect result;
    result.x = window->rect.x + window->rect.width -
            (windowCloseButtonSize + windowBorderSize);
    result.y = window->rect.y + windowBorderSize;
    result.width = windowCloseButtonSize;
    result.height = windowCloseButtonSize;
    return result;
}

void hideWindow(struct Window* window) {
    if (!window->visible) return;
    window->visible = false;
    addDamageRect(window->rect);
}

void moveWindowToTop(struct Window* window) {
    if (window == topWindow) return;
    removeWindow(window);
    addWindowOnTop(window);
}

void redrawWindow(struct Window* window, int width, int height,
        dxui_color* lfb) {
    if (window->clientDim.width != width ||
            window->clientDim.height != height) {
        free(window->lfb);
        window->lfb = malloc(width * height * sizeof(dxui_color));
        if (!window->lfb) dxui_panic(context, "malloc");
        window->clientDim.width = width;
        window->clientDim.height = height;
    }
    memcpy(window->lfb, lfb, width * height * sizeof(dxui_color));
    if (window->visible) {
        addDamageRect(getClientRect(window));
    }
}

void redrawWindowPart(struct Window* window, int x, int y, int width,
        int height, size_t pitch, dxui_color* lfb) {
    if (x + width > window->clientDim.width ||
            y + height > window->clientDim.height) {
        return;
    }

    for (int yPos = 0; yPos < height; yPos++) {
        for (int xPos = 0; xPos < width; xPos++) {
            size_t index = (y + yPos) * window->clientDim.width + x + xPos;
            window->lfb[index] = lfb[yPos * pitch + xPos];
        }
    }

    if (window->visible) {
        dxui_rect rect;
        rect.x = getClientRect(window).x + x;
        rect.y = getClientRect(window).y + y;
        rect.width = width;
        rect.height = height;
        addDamageRect(rect);
    }
}

static void removeWindow(struct Window* window) {
    if (window->below) {
        window->below->above = window->above;
    }
    if (window->above) {
        window->above->below = window->below;
    } else {
        topWindow = window->below;
        if (window->relativeMouse != (topWindow && topWindow->relativeMouse)) {
            dxui_set_relative_mouse(compositorWindow, topWindow &&
                    topWindow->relativeMouse);
        }
    }
}

dxui_color renderClientArea(struct Window* window, int x, int y) {
    if (x >= 0 && x < window->clientDim.width && y >= 0 &&
            y < window->clientDim.height) {
        return window->lfb[y * window->clientDim.width + x];
    } else {
        return window->background;
    }
}

static dxui_color renderCloseButton(int x, int y) {
    if (((x == y) || (y == windowCloseButtonSize - 1 - x)) && x > 2 && x < 13) {
        return closeCrossColor;
    } else {
        return closeButtonColor;
    }
}

dxui_color renderWindowDecoration(struct Window* window, int x, int y) {
    int titleBegin = (window->rect.width - window->titleDim.width) / 2;

    if (y < windowBorderSize ||
            y >= windowBorderSize + window->titleDim.height) {
        return windowDecorationColor;
    } else if (x >= window->rect.width - (windowBorderSize +
            windowCloseButtonSize) && x < window->rect.width -
            windowBorderSize) {
        return renderCloseButton(x - window->rect.width + windowBorderSize +
                windowCloseButtonSize, y - windowBorderSize);
    } else if (x < titleBegin ||
            x >= titleBegin + window->titleDim.width) {
        return windowDecorationColor;
    } else {
        x -= titleBegin;
        y -= windowBorderSize;
        dxui_color color = window->titleLfb[y * window->titleDim.width + x];
        if (!color) return windowDecorationColor;
        return color;

    }
}

void resizeClientRect(struct Window* window, dxui_dim dim) {
    dxui_rect rect = window->rect;
    rect.width = dim.width + 2 * windowBorderSize;
    rect.height = dim.height + windowTitleBarSize + windowBorderSize;
    resizeWindow(window, rect);
}

void resizeWindow(struct Window* window, dxui_rect rect) {
    if (window->visible) {
        addDamageRect(window->rect);
        addDamageRect(rect);
    }
    window->rect = rect;

    struct gui_event_window_resized msg;
    msg.window_id = window->id;
    msg.width = getClientRect(window).width;
    msg.height = getClientRect(window).height;
    sendEvent(window->connection, GUI_EVENT_WINDOW_RESIZED, sizeof(msg), &msg);
}

void setWindowBackground(struct Window* window, dxui_color color) {
    window->background = color;
    addDamageRect(window->rect);
}

void setWindowCursor(struct Window* window, int cursor) {
    window->cursor = cursor;
}

void setWindowTitle(struct Window* window, const char* title) {
    free(window->titleLfb);
    dxui_rect rect = {{0, 0, 0, 0}};
    rect = dxui_get_text_rect(title, rect, 0);
    window->titleLfb = calloc(rect.width * rect.height, sizeof(dxui_color));
    if (!window->titleLfb) dxui_panic(context, "malloc");
    dxui_draw_text_in_rect(context, window->titleLfb, title, titleColor,
            rect.pos, rect, rect.width);
    window->titleDim = rect.dim;

    dxui_rect titleBar = window->rect;
    titleBar.height = windowTitleBarSize;
    addDamageRect(titleBar);
}

void showWindow(struct Window* window) {
    if (window->visible) return;
    window->visible = true;
    addDamageRect(window->rect);
}
