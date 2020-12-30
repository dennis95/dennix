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

/* gui/display.c
 * Display.
 */

#include <devctl.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <dennix/display.h>

#include "display.h"
#include "gui.h"
#include "mouse.h"
#include "window.h"

static const uint32_t backgroundColor = RGB(0, 200, 255);

static int displayFd;
static uint32_t* lfb;
static int oldMode;

struct Rectangle damageRect;
struct Rectangle displayRect;

static uint32_t blend(uint32_t fg, uint32_t bg);
static void onSignal(int signo);
static uint32_t renderPixel(int x, int y);
static void restoreDisplay(void);

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))
#define RED_PART(rgba) (((rgba) >> 16) & 0xFF)
#define GREEN_PART(rgba) (((rgba) >> 8) & 0xFF)
#define BLUE_PART(rgba) (((rgba) >> 0) & 0xFF)
#define ALPHA_PART(rgba) (((rgba) >> 24) & 0xFF)

void addDamageRect(struct Rectangle rect) {
    // TODO: This is a rather primitive implementation that causes us to redraw
    // too much.
    if (damageRect.width == 0) {
        damageRect = rect;
        return;
    }
    if (rect.width == 0) return;

    struct Rectangle newRect;
    newRect.x = min(damageRect.x, rect.x);
    newRect.y = min(damageRect.y, rect.y);
    int xEnd = max(damageRect.x + damageRect.width, rect.x + rect.width);
    int yEnd = max(damageRect.y + damageRect.height, rect.y + rect.height);
    newRect.width = xEnd - newRect.x;
    newRect.height = yEnd - newRect.y;
    damageRect = newRect;
}

static uint32_t blend(uint32_t fg, uint32_t bg) {
    uint32_t r = RED_PART(fg);
    uint32_t g = GREEN_PART(fg);
    uint32_t b = BLUE_PART(fg);
    uint32_t a = ALPHA_PART(fg);

    r = r * a * 255 + RED_PART(bg) * ALPHA_PART(bg) * (255 - a);
    g = g * a * 255 + GREEN_PART(bg) * ALPHA_PART(bg) * (255 - a);
    b = b * a * 255 + BLUE_PART(bg) * ALPHA_PART(bg) * (255 - a);
    a = a * 255 + ALPHA_PART(bg) * (255 - a);
    return RGBA(r / 255 / 255, g / 255 / 255, b / 255 / 255, a / 255);
}

void composit(void) {
    struct Rectangle rect = damageRect;

    if (rect.x < 0) {
        rect.width += rect.x;
        rect.x = 0;
    }
    if (rect.x + rect.width >= displayRect.width) {
        rect.width = displayRect.width - rect.x;
    }
    if (rect.y < 0) {
        rect.height += rect.y;
        rect.y = 0;
    }
    if (rect.y + rect.height >= displayRect.height) {
        rect.height = displayRect.height - rect.y;
    }

    for (int y = rect.y; y < rect.y + rect.height; y++) {
        for (int x = rect.x; x < rect.x + rect.width; x++) {
            lfb[y * displayRect.width + x] = renderPixel(x, y);
        }
    }

    struct display_draw draw;
    draw.lfb = lfb;
    draw.lfb_pitch = displayRect.width * sizeof(uint32_t);
    draw.lfb_x = 0;
    draw.lfb_y = 0;
    draw.draw_x = rect.x;
    draw.draw_y = rect.y;
    draw.draw_width = rect.width;
    draw.draw_height = rect.height;
    posix_devctl(displayFd, DISPLAY_DRAW, &draw, sizeof(draw), NULL);

    damageRect.width = 0;
}

void initializeDisplay(void) {
    displayFd = open("/dev/display", O_RDONLY | O_CLOEXEC);
    if (displayFd < 0) err(1, "cannot open '/dev/display'");
    int mode = DISPLAY_MODE_QUERY;
    errno = posix_devctl(displayFd, DISPLAY_SET_MODE, &mode, sizeof(mode),
            &oldMode);
    if (errno) err(1, "cannot get display mode");

    atexit(restoreDisplay);
    signal(SIGABRT, onSignal);
    signal(SIGBUS, onSignal);
    signal(SIGFPE, onSignal);
    signal(SIGILL, onSignal);
    signal(SIGINT, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGSEGV, onSignal);
    signal(SIGTERM, onSignal);

    mode = DISPLAY_MODE_LFB;
    errno = posix_devctl(displayFd, DISPLAY_SET_MODE, &mode, sizeof(mode),
            NULL);
    if (errno) err(1, "cannot set display mode");

    struct display_resolution res;
    posix_devctl(displayFd, DISPLAY_GET_RESOLUTION, &res, sizeof(res), NULL);
    displayRect.x = 0;
    displayRect.y = 0;
    displayRect.width = res.width;
    displayRect.height = res.height;
    mouseX = displayRect.width / 2;
    mouseY = displayRect.height / 2;

    lfb = malloc(displayRect.width * displayRect.height * sizeof(uint32_t));
    if (!lfb) err(1, "malloc");
}

static void onSignal(int signo) {
    restoreDisplay();
    signal(signo, SIG_DFL);
    raise(signo);
}

static uint32_t renderPixel(int x, int y) {
    uint32_t rgba = 0;
    struct Rectangle mouseRect = getMouseRect();
    if (isInRect(x, y, mouseRect)) {
        int xPixel = x - mouseRect.x;
        int yPixel = y - mouseRect.y;
        rgba = renderCursor(xPixel, yPixel);
    }

    for (struct Window* window = topWindow; window; window = window->below) {
        if (!window->visible) continue;
        if (!isInRect(x, y, window->rect)) continue;
        struct Rectangle clientRect = getClientRect(window);

        uint32_t color;
        if (isInRect(x, y, clientRect)) {
            color = renderClientArea(window, x - clientRect.x,
                    y - clientRect.y);
        } else {
            color = renderWindowDecoration(window, x - window->rect.x,
                    y - window->rect.y);
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

static void restoreDisplay(void) {
    posix_devctl(displayFd, DISPLAY_SET_MODE, &oldMode, sizeof(oldMode), NULL);
}
