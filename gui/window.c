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

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <dennix/display.h>

#include "connection.h"
#include "display.h"
#include "gui.h"
#include "window.h"

static const int fontHeight = 16;
static const int fontWidth = 9;
static const int windowBorderSize = 4;
static const int windowCloseButtonSize = 16;
static const int windowTitleBarSize = fontHeight + 2 * windowBorderSize;

static const uint32_t closeButtonColor = RGB(255, 0, 0);
static const uint32_t closeCrossColor = RGB(255, 255, 255);
static const uint32_t titleColor = RGB(0, 0, 0);
static const uint32_t windowDecorationColor = RGBA(64, 64, 180, 200);

struct Window* changingWindow;
struct Window* mouseWindow;
struct Window* topWindow;

static void addWindowOnTop(struct Window* window);
static struct Rectangle chooseWindowRect(int x, int y, int width, int height);
static struct Rectangle getCloseButtonRect(struct Window* window);
static void removeWindow(struct Window* window);
static uint32_t renderCloseButton(int x, int y);

static void addWindowOnTop(struct Window* window) {
    if (topWindow) {
        topWindow->above = window;
    }
    window->below = topWindow;
    window->above = NULL;
    topWindow = window;

    if (window->visible) {
        addDamageRect(window->rect);
    }
}

struct Window* addWindow(int x, int y, int width, int height, char* title,
        int flags, struct Connection* connection) {
    struct Window* window = malloc(sizeof(struct Window));
    if (!window) err(1, "malloc");

    window->connection = connection;
    window->background = RGB(245, 245, 245);
    window->cursor = GUI_CURSOR_ARROW;
    window->flags = flags;
    window->rect = chooseWindowRect(x, y, width, height);
    window->title = title;
    window->titlePixelLength = strlen(title) * fontWidth - 1;
    window->lfb = NULL;
    window->clientWidth = 0;
    window->clientHeight = 0;
    window->visible = false;

    addWindowOnTop(window);
    return window;
}

int checkMouseInteraction(struct Window* window, int x, int y) {
    if (isInRect(x, y, window->rect)) {
        if (isInRect(x, y, getClientRect(window))) {
            return CLIENT_AREA;
        } else if (isInRect(x, y, getCloseButtonRect(window))) {
            return CLOSE_BUTTON;
        } else {
            int result = 0;
            if (!(window->flags & GUI_WINDOW_NO_RESIZE)) {
                if (x - window->rect.x < windowBorderSize) {
                    result |= RESIZE_LEFT;
                }
                if (x - window->rect.x >= window->rect.width -
                        windowBorderSize) {
                    result |= RESIZE_RIGHT;
                }
                if (y - window->rect.y < windowBorderSize) {
                    result |= RESIZE_TOP;
                }
                if (y - window->rect.y >= window->rect.height -
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

static struct Rectangle chooseWindowRect(int x, int y, int width, int height) {
    struct Rectangle rect;
    rect.x = x;
    rect.y = y;
    rect.width = width + 2 * windowBorderSize;
    rect.height = height + windowTitleBarSize + windowBorderSize;

    if (x < 0) {
        int maxX = displayRect.width - rect.width;
        if (maxX < 0) maxX = 50;
        rect.x = arc4random_uniform(maxX + 1);
    }

    if (y < 0) {
        int maxY = displayRect.height - rect.height;
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
    free(window->title);
    free(window->lfb);
    free(window);

    if (topWindow == NULL) {
        // If all windows have been closed, exit the gui.
        exit(0);
    }
}

struct Rectangle getClientRect(struct Window* window) {
    struct Rectangle result;
    result.x = window->rect.x + windowBorderSize;
    result.y = window->rect.y + windowTitleBarSize;
    result.width = window->rect.width - 2 * windowBorderSize;
    result.height = window->rect.height - windowTitleBarSize - windowBorderSize;
    return result;
}

static struct Rectangle getCloseButtonRect(struct Window* window) {
    struct Rectangle result;
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

void redrawWindow(struct Window* window, int width, int height, uint32_t* lfb) {
    if (window->clientWidth != width || window->clientHeight != height) {
        free(window->lfb);
        window->lfb = malloc(width * height * sizeof(uint32_t));
        if (!window->lfb) err(1, "malloc");
        window->clientWidth = width;
        window->clientHeight = height;
    }
    memcpy(window->lfb, lfb, width * height * sizeof(uint32_t));
    if (window->visible) {
        addDamageRect(getClientRect(window));
    }
}

void redrawWindowPart(struct Window* window, int x, int y, int width,
        int height, size_t pitch, uint32_t* lfb) {
    if (x + width > window->clientWidth || y + height > window->clientHeight) {
        return;
    }

    for (int yPos = 0; yPos < height; yPos++) {
        for (int xPos = 0; xPos < width; xPos++) {
            size_t index = (y + yPos) * window->clientWidth + x + xPos;
            window->lfb[index] = lfb[yPos * pitch + xPos];
        }
    }

    if (window->visible) {
        struct Rectangle rect;
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
    }
}

uint32_t renderClientArea(struct Window* window, int x, int y) {
    if (x >= 0 && x < window->clientWidth && y >= 0 &&
            y < window->clientHeight) {
        return window->lfb[y * window->clientWidth + x];
    } else {
        return window->background;
    }
}

static uint32_t renderCloseButton(int x, int y) {
    if (((x == y) || (y == windowCloseButtonSize - 1 - x)) && x > 2 && x < 13) {
        return closeCrossColor;
    } else {
        return closeButtonColor;
    }
}

uint32_t renderWindowDecoration(struct Window* window, int x, int y) {
    int titleBegin = (window->rect.width - window->titlePixelLength) / 2;

    if (y < windowBorderSize || y >= windowBorderSize + fontHeight) {
        return windowDecorationColor;
    } else if (x >= window->rect.width - (windowBorderSize +
            windowCloseButtonSize) && x < window->rect.width -
            windowBorderSize) {
        return renderCloseButton(x - window->rect.width + windowBorderSize +
                windowCloseButtonSize, y - windowBorderSize);
    } else if (x < titleBegin ||
            x > titleBegin + (int) window->titlePixelLength) {
        return windowDecorationColor;
    } else {
        size_t i = (x - titleBegin) / fontWidth;
        unsigned char c = window->title[i];
        const char* font = &vgafont[c * fontHeight];
        size_t j = y - windowBorderSize;
        size_t k = (x - titleBegin) % fontWidth;
        if (k != 8 && font[j] & (1 << (7 - k))) {
            return titleColor;
        } else {
            return windowDecorationColor;
        }
    }
}

void resizeWindow(struct Window* window, struct Rectangle rect) {
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

void setWindowBackground(struct Window* window, uint32_t color) {
    window->background = color;
    addDamageRect(window->rect);
}

void setWindowCursor(struct Window* window, int cursor) {
    window->cursor = cursor;
}

void setWindowTitle(struct Window* window, char* title) {
    free(window->title);
    window->title = title;
    window->titlePixelLength = strlen(title) * fontWidth - 1;
    struct Rectangle titleBar = window->rect;
    titleBar.height = windowTitleBarSize;
    addDamageRect(titleBar);
}

void showWindow(struct Window* window) {
    if (window->visible) return;
    window->visible = true;
    addDamageRect(window->rect);
}
