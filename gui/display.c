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

/* gui/display.c
 * Display.
 */

#include <stdlib.h>
#include "window.h"

static dxui_rect damageRect;
static dxui_color* text1FrameBuffer;
static dxui_color* text2FrameBuffer;
static dxui_color* text3FrameBuffer;
static dxui_rect text1Rect;
static dxui_rect text2Rect;
static dxui_rect text3Rect;

static dxui_color blend(dxui_color fg, dxui_color bg);
static dxui_color renderPixel(dxui_pos pos);

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))
#define RED_PART(rgba) (((rgba) >> 16) & 0xFF)
#define GREEN_PART(rgba) (((rgba) >> 8) & 0xFF)
#define BLUE_PART(rgba) (((rgba) >> 0) & 0xFF)
#define ALPHA_PART(rgba) (((rgba) >> 24) & 0xFF)

void addDamageRect(dxui_rect rect) {
    // TODO: This is a rather primitive implementation that causes us to redraw
    // too much.
    if (damageRect.width == 0) {
        damageRect = rect;
        return;
    }
    if (rect.width == 0) return;

    dxui_rect newRect;
    newRect.x = min(damageRect.x, rect.x);
    newRect.y = min(damageRect.y, rect.y);
    int xEnd = max(damageRect.x + damageRect.width, rect.x + rect.width);
    int yEnd = max(damageRect.y + damageRect.height, rect.y + rect.height);
    newRect.width = xEnd - newRect.x;
    newRect.height = yEnd - newRect.y;
    damageRect = newRect;
}

static dxui_color blend(dxui_color fg, dxui_color bg) {
    if (ALPHA_PART(fg) == 0) return bg;
    if (ALPHA_PART(bg) == 0) return fg;

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

void composit(void) {
    dxui_rect rect = dxui_rect_crop(damageRect, guiDim);
    if (rect.width == 0) return;

    for (int y = rect.y; y < rect.y + rect.height; y++) {
        for (int x = rect.x; x < rect.x + rect.width; x++) {
            lfb[y * guiDim.width + x] = renderPixel((dxui_pos) {x, y});
        }
    }

    dxui_update_framebuffer(compositorWindow, rect);
    damageRect.width = 0;
}

void handleResize(dxui_window* window, dxui_resize_event* event) {
    lfb = dxui_get_framebuffer(window, event->dim);
    if (!lfb) dxui_panic(context, "Failed to create window framebuffer");
    guiDim = event->dim;

    dxui_rect rect = { .pos = {0, 0}, .dim = guiDim };
    addDamageRect(rect);

    for (struct Window* win = topWindow; win; win = win->below) {
        if (win->rect.x > guiDim.width - 10) {
            win->rect.x = guiDim.width - 50;
        }

        if (win->rect.y > guiDim.height - 10) {
            win->rect.y = guiDim.height - 50;
        }
    }

    text3Rect.x = guiDim.width - text3Rect.width - 5;
    text3Rect.y = guiDim.height - text3Rect.height - 5;

    broadcastStatusEvent();
}

void initializeDisplay(void) {
    const char* text1 = "Press GUI key + T to open a terminal.";
    const char* text2 = "Press GUI key + Q to quit the compositor.";
    const char* text3 = "Dennix " DENNIX_VERSION;

    dxui_rect rect = {0};
    rect = dxui_get_text_rect(text1, rect, 0);
    text1FrameBuffer = calloc(rect.width, rect.height * sizeof(dxui_color));
    if (!text1FrameBuffer) dxui_panic(context, "malloc");
    dxui_draw_text_in_rect(context, text1FrameBuffer, text1, COLOR_WHITE,
            rect.pos, rect, rect.width);
    text1Rect.pos = (dxui_pos) { 5, 5 };
    text1Rect.dim = rect.dim;

    rect = dxui_get_text_rect(text2, rect, 0);
    text2FrameBuffer = calloc(rect.width, rect.height * sizeof(dxui_color));
    if (!text2FrameBuffer) dxui_panic(context, "malloc");
    dxui_draw_text_in_rect(context, text2FrameBuffer, text2, COLOR_WHITE,
            rect.pos, rect, rect.width);
    text2Rect.pos = (dxui_pos) { 5, 21 };
    text2Rect.dim = rect.dim;

    rect = dxui_get_text_rect(text3, rect, 0);
    text3FrameBuffer = calloc(rect.width, rect.height * sizeof(dxui_color));
    if (!text3FrameBuffer) dxui_panic(context, "malloc");
    dxui_draw_text_in_rect(context, text3FrameBuffer, text3, COLOR_WHITE,
            rect.pos, rect, rect.width);
    text3Rect.x = guiDim.width - rect.width - 5;
    text3Rect.y = guiDim.height - rect.height - 5;
    text3Rect.dim = rect.dim;
}

static dxui_color renderPixel(dxui_pos pos) {
    dxui_color rgba = 0;

    for (struct Window* window = topWindow; window; window = window->below) {
        if (!window->visible) continue;
        if (!dxui_rect_contains_pos(window->rect, pos)) continue;
        dxui_rect clientRect = getClientRect(window);

        dxui_color color;
        if (dxui_rect_contains_pos(clientRect, pos)) {
            color = renderClientArea(window, pos.x - clientRect.x,
                    pos.y - clientRect.y);
        } else {
            color = renderWindowDecoration(window, pos.x - window->rect.x,
                    pos.y - window->rect.y);
        }
        rgba = blend(rgba, color);
        if (ALPHA_PART(rgba) == 255) return rgba;
    }

    if (dxui_rect_contains_pos(text1Rect, pos)) {
        dxui_color color = text1FrameBuffer[pos.x - text1Rect.x +
                text1Rect.width * (pos.y - text1Rect.y)];
        rgba = blend(rgba, color);
    } else if (dxui_rect_contains_pos(text2Rect, pos)) {
        dxui_color color = text2FrameBuffer[pos.x - text2Rect.x +
                text2Rect.width * (pos.y - text2Rect.y)];
        rgba = blend(rgba, color);
    } else if (dxui_rect_contains_pos(text3Rect, pos)) {
        dxui_color color = text3FrameBuffer[pos.x - text3Rect.x +
                text3Rect.width * (pos.y - text3Rect.y)];
        rgba = blend(rgba, color);
    }

    if (ALPHA_PART(rgba) == 255) {
        return rgba;
    } else {
        return blend(rgba, backgroundColor);
    }
}
