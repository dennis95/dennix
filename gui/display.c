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

#include "window.h"

static const dxui_color backgroundColor = RGB(0, 200, 255);
static dxui_rect damageRect;

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
        if (ALPHA_PART(rgba) == 255) {
            return rgba;
        } else if (ALPHA_PART(rgba) == 0) {
            rgba = color;
        } else {
            rgba = blend(rgba, color);
        }
    }

    if (ALPHA_PART(rgba) == 255) {
        return rgba;
    } else if (ALPHA_PART(rgba) == 0) {
        return backgroundColor;
    } else {
        return blend(rgba, backgroundColor);
    }
}
