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

/* gui/window.h
 * Window.
 */

#ifndef WINDOW_H
#define WINDOW_H

#include "gui.h"

struct Window {
    struct Window* above;
    struct Window* below;
    struct Connection* connection;
    dxui_color background;
    int cursor;
    unsigned int id;
    int flags;
    dxui_rect rect;
    dxui_color* titleLfb;
    dxui_dim titleDim;
    dxui_color* lfb;
    dxui_dim clientDim;
    bool relativeMouse;
    bool visible;
};

enum {
    RESIZE_TOP = 1 << 0,
    RESIZE_RIGHT = 1 << 1,
    RESIZE_BOTTOM = 1 << 2,
    RESIZE_LEFT = 1 << 3,
    CLIENT_AREA = 1 << 4,
    CLOSE_BUTTON,
    TITLE_BAR,

    RESIZE_TOP_LEFT = RESIZE_TOP | RESIZE_LEFT,
    RESIZE_TOP_RIGHT = RESIZE_TOP | RESIZE_RIGHT,
    RESIZE_BOTTOM_LEFT = RESIZE_BOTTOM | RESIZE_LEFT,
    RESIZE_BOTTOM_RIGHT = RESIZE_BOTTOM | RESIZE_RIGHT,
};

extern struct Window* changingWindow;
extern struct Window* mouseWindow;
extern struct Window* topWindow;

struct Window* addWindow(int x, int y, int width, int height, const char* title,
        int flags, struct Connection* connection);
int checkMouseInteraction(struct Window* window, dxui_pos pos);
void closeWindow(struct Window* window);
dxui_rect getClientRect(struct Window* window);
void hideWindow(struct Window* window);
void moveWindowToTop(struct Window* window);
void redrawWindow(struct Window* window, int width, int height,
        dxui_color* lfb);
void redrawWindowPart(struct Window* window, int x, int y, int width,
        int height, size_t pitch, dxui_color* lfb);
dxui_color renderClientArea(struct Window* window, int x, int y);
dxui_color renderWindowDecoration(struct Window* window, int x, int y);
void resizeClientRect(struct Window* window, dxui_dim dim);
void resizeWindow(struct Window* window, dxui_rect rect);
void setWindowBackground(struct Window* window, dxui_color color);
void setWindowCursor(struct Window* window, int cursor);
void setWindowTitle(struct Window* window, const char* title);
void showWindow(struct Window* window);

#endif
