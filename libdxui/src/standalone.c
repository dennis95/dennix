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

/* libdxui/src/standalone.c
 * Standalone backend.
 */

#include <devctl.h>
#include <stdlib.h>
#include <dennix/display.h>
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

const Backend dxui_standaloneBackend = {
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

static Window* getWindow(dxui_context* context, unsigned int id) {
    for (Window* win = context->firstWindow; win; win = win->next) {
        if (win->idAssigned && win->id == id) {
            return win;
        }
    }

    return NULL;
}

static void readjustViewport(dxui_context* context) {
    dxui_dim dim = dxui_get_dim(context->activeWindow);
    dxui_dim displayDim = context->displayDim;
    if (dim.width > displayDim.width) {
        dim.width = displayDim.width;
    }
    if (dim.height > displayDim.height) {
        dim.height = displayDim.height;
    }

    context->viewport.x = (displayDim.width - dim.width) / 2;
    context->viewport.y = (displayDim.height - dim.height) / 2;
}

static void setActiveWindow(dxui_context* context, Window* window) {
    context->activeWindow = window;
    if (!window) return;
    readjustViewport(context);
    dxui_update(window);
}

static void closeWindow(dxui_context* context, unsigned int id) {
    Window* window = getWindow(context, id);
    if (window == context->activeWindow) {
        setActiveWindow(context, getWindow(context,
                window->prevActiveWindowId));
    }
}

static void createWindow(dxui_context* context, dxui_rect rect,
        const char* title, int flags) {
    (void) rect; (void) title; (void) flags;

    for (Window* win = context->firstWindow; win; win = win->next) {
        if (!win->idAssigned) {
            win->id = context->idCounter++;
            win->idAssigned = true;
            return;
        }
    }
}

static void hideWindow(dxui_context* context, unsigned int id) {
    Window* window = getWindow(context, id);
    if (window == context->activeWindow) {
        setActiveWindow(context, getWindow(context,
                window->prevActiveWindowId));
    }
}

static void resizeWindow(dxui_context* context, unsigned int id, dxui_dim dim) {
    Window* window = getWindow(context, id);
    dxui_color* newLfb = malloc(dim.width * dim.height * sizeof(dxui_color));
    if (!newLfb) return;
    free(window->lfb);
    window->lfb = newLfb;
    window->lfbDim = dim;
    window->redraw = true;

    void (*handler)(dxui_window*, dxui_resize_event*) =
            window->control.eventHandlers[DXUI_EVENT_WINDOW_RESIZED];
    if (handler) {
        dxui_resize_event event;
        event.dim = dim;
        handler(DXUI_AS_WINDOW(window), &event);
    }
    dxui_update(window);
}

static void setWindowCursor(dxui_context* context, unsigned int id,
        int cursor) {
    if (cursor > 4) return;
    Window* window = getWindow(context, id);
    window->cursor = cursor;
    if (window == context->activeWindow) {
        dxui_update(window);
    }
}

static void showWindow(dxui_context* context, unsigned int id) {
    Window* window = getWindow(context, id);
    if (window == context->activeWindow) return;

    if (context->activeWindow) {
        window->prevActiveWindowId = context->activeWindow->id;
    } else {
        window->prevActiveWindowId = -1;
    }
    setActiveWindow(context, window);
}

static void setWindowBackground(dxui_context* context, unsigned int id,
        dxui_color color) {
    (void) context; (void) id; (void) color;
}

static void setWindowTitle(dxui_context* context, unsigned int id,
        const char* title) {
    (void) context; (void) id; (void) title;
}

#define RED_PART(rgba) (((rgba) >> 16) & 0xFF)
#define GREEN_PART(rgba) (((rgba) >> 8) & 0xFF)
#define BLUE_PART(rgba) (((rgba) >> 0) & 0xFF)
#define ALPHA_PART(rgba) (((rgba) >> 24) & 0xFF)

static dxui_color blend(dxui_color fg, dxui_color bg) {
    dxui_color r = RED_PART(fg);
    dxui_color g = GREEN_PART(fg);
    dxui_color b = BLUE_PART(fg);
    dxui_color a = ALPHA_PART(fg);

    r = r * a * 255 + RED_PART(bg) * ALPHA_PART(bg) * (255 - a);
    g = g * a * 255 + GREEN_PART(bg) * ALPHA_PART(bg) * (255 - a);
    b = b * a * 255 + BLUE_PART(bg) * ALPHA_PART(bg) * (255 - a);
    a = a * 255 + ALPHA_PART(bg) * (255 - a);
    return RGBA(r / 255 / 255, g / 255 / 255, b / 255 / 255, a / 255);
}

static void draw(dxui_context* context, dxui_rect rect) {
    dxui_dim displayDim = context->displayDim;

    if (context->cursors) {
        dxui_rect cursorRect;
        cursorRect.x = context->mousePos.x - 24;
        cursorRect.y = context->mousePos.y - 24;
        cursorRect.width = 48;
        cursorRect.height = 48;

        dxui_color* cursor = context->cursors +
                (48 * 48 * context->activeWindow->cursor);

        dxui_rect r = dxui_rect_intersect(cursorRect, rect);
        for (int y = r.y; y < r.y + r.height; y++) {
            for (int x = r.x; x < r.x + r.width; x++) {
                dxui_color c = cursor[(y - cursorRect.y) * 48 +
                        (x - cursorRect.x)];
                if (ALPHA_PART(c) == 0) continue;
                context->framebuffer[y * displayDim.width + x] = blend(c,
                        context->framebuffer[y * displayDim.width + x]);
            }
        }
    }

    struct display_draw draw;
    draw.lfb = context->framebuffer;
    draw.lfb_pitch = displayDim.width * sizeof(uint32_t);
    draw.lfb_x = 0;
    draw.lfb_y = 0;
    draw.draw_x = rect.x;
    draw.draw_y = rect.y;
    draw.draw_width = rect.width;
    draw.draw_height = rect.height;
    posix_devctl(context->displayFd, DISPLAY_DRAW, &draw, sizeof(draw), NULL);
}

static void redrawWindow(dxui_context* context, unsigned int id, dxui_dim dim,
        dxui_color* lfb) {
    if (getWindow(context, id) != context->activeWindow) return;

    readjustViewport(context);
    dxui_rect rect;
    rect.pos = context->viewport;
    rect.dim = dim;

    dxui_dim displayDim = context->displayDim;
    rect = dxui_rect_crop(rect, displayDim);

    for (int i = 0; i < displayDim.width * displayDim.height; i++) {
        context->framebuffer[i] = COLOR_BLACK;
    }

    size_t pitch = dim.width;
    for (int y = rect.y; y < rect.y + rect.height; y++) {
        for (int x = rect.x; x < rect.x + rect.width; x++) {
            context->framebuffer[y * displayDim.width + x] =
                    lfb[(y - rect.y) * pitch + (x - rect.x)];
        }
    }

    rect.x = 0;
    rect.y = 0;
    rect.dim = displayDim;
    draw(context, rect);
}

static void redrawWindowPart(dxui_context* context, unsigned int id,
        unsigned int pitch, dxui_rect rect, dxui_color* lfb) {
    if (getWindow(context, id) != context->activeWindow) return;

    dxui_dim displayDim = context->displayDim;

    for (int y = rect.y; y < rect.y + rect.height; y++) {
        for (int x = rect.x; x < rect.x + rect.width; x++) {
            context->framebuffer[(context->viewport.y + y) *
                    displayDim.width + x + context->viewport.x] =
                    lfb[y * pitch + x];
        }
    }

    rect.x += context->viewport.x;
    rect.y += context->viewport.y;
    draw(context, rect);
}
