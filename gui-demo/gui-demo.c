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

/* gui-demo/gui-demo.c
 * GUI demo.
 */

#include <devctl.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dennix/display.h>
#include <dennix/mouse.h>

struct Rectangle {
    int x;
    int y;
    int width;
    int height;
};

struct Window {
    struct Window* above;
    struct Window* below;
    uint32_t color;
    struct Rectangle rect;
    const char* title;
    size_t titlePixelLength;
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

static const uint32_t backgroundColor = RGB(0, 200, 255);
static const uint32_t closeButtonColor = RGB(255, 0, 0);
static const uint32_t closeCrossColor = RGB(255, 255, 255);
static const uint32_t titleColor = RGB(0, 0, 0);
static const uint32_t windowDecorationColor = RGBA(64, 64, 180, 200);

static const int cursorSize = 48;
static const int fontHeight = 16;
static const int fontWidth = 9;
static const int minimumWindowHeight = 100;
static const int minimumWindowWidth = 100;
static const int windowBorderSize = 4;
static const int windowCloseButtonSize = 16;
static const int windowTitleBarSize = fontHeight + 2 * windowBorderSize;

static uint32_t* cursor;
static uint32_t arrowCursor[48 * 48];
static struct Window* changingWindow;
static int displayFd;
static struct Rectangle damageRect;
static struct Rectangle displayRect;
static bool leftClick;
static uint32_t* lfb;
static int mouseFd;
static int mouseX;
static int mouseY;
static int oldMode;
static int resizeDirection;
static uint32_t resizeD1Cursor[48 * 48];
static uint32_t resizeD2Cursor[48 * 48];
static uint32_t resizeHCursor[48 * 48];
static uint32_t resizeVCursor[48 * 48];
static struct Window* topWindow;
static char vgafont[4096];

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))
#define RED_PART(rgba) (((rgba) >> 16) & 0xFF)
#define GREEN_PART(rgba) (((rgba) >> 8) & 0xFF)
#define BLUE_PART(rgba) (((rgba) >> 0) & 0xFF)
#define ALPHA_PART(rgba) (((rgba) >> 24) & 0xFF)

static void addDamageRect(struct Rectangle rect);
static void addWindowOnTop(struct Window* window);
static void addWindow(struct Rectangle rect, uint32_t color, const char* title);
static uint32_t blend(uint32_t fg, uint32_t bg);
static int checkMouseInteraction(struct Window** window);
static void closeWindow(struct Window* window);
static void composit(struct Rectangle rect);
static void eventLoop(void);
static struct Rectangle getClientRect(struct Window* window);
static struct Rectangle getCloseButtonRect(struct Window* window);
static struct Rectangle getMouseRect(void);
static void handleMouse(void);
static void initialize(void);
static bool isInRect(int x, int y, struct Rectangle rect);
static void loadAssets(void);
static void loadFromFile(const char* filename, void* buffer, size_t size);
static void moveWindowToTop(struct Window* window);
static void onSignal(int signo);
static void removeWindow(struct Window* window);
static uint32_t renderCloseButton(int x, int y);
static uint32_t renderPixel(int x, int y);
static uint32_t renderWindowDecoration(struct Window* window, int x, int y);
static void resizeWindow(struct Window* window, struct Rectangle rect);
static void restoreDisplay(void);

static void addDamageRect(struct Rectangle rect) {
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

static void addWindowOnTop(struct Window* window) {
    if (topWindow) {
        topWindow->above = window;
    }
    window->below = topWindow;
    window->above = NULL;
    topWindow = window;
}

static void addWindow(struct Rectangle rect, uint32_t color,
        const char* title) {
    struct Window* window = malloc(sizeof(struct Window));
    if (!window) err(1, "malloc");

    window->rect = rect;
    window->color = color;
    window->title = title;
    window->titlePixelLength = strlen(title) * fontWidth - 1;

    addWindowOnTop(window);
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

static int checkMouseInteraction(struct Window** window) {
    for (struct Window* win = topWindow; win; win = win->below) {
        if (isInRect(mouseX, mouseY, win->rect)) {
            if (window) {
                *window = win;
            }

            if (isInRect(mouseX, mouseY, getClientRect(win))) {
                return CLIENT_AREA;
            } else if (isInRect(mouseX, mouseY, getCloseButtonRect(win))) {
                return CLOSE_BUTTON;
            } else {
                int result = 0;
                if (mouseX - win->rect.x < windowBorderSize) {
                    result |= RESIZE_LEFT;
                }
                if (mouseX - win->rect.x >= win->rect.width -
                        windowBorderSize) {
                    result |= RESIZE_RIGHT;
                }
                if (mouseY - win->rect.y < windowBorderSize) {
                    result |= RESIZE_TOP;
                }
                if (mouseY - win->rect.y >= win->rect.height -
                        windowBorderSize) {
                    result |= RESIZE_BOTTOM;
                }
                return result ? result : TITLE_BAR;
            }
        }
    }

    if (window) {
        *window = NULL;
    }
    return 0;
}

static void closeWindow(struct Window* window) {
    removeWindow(window);
    addDamageRect(window->rect);
    free(window);

    if (topWindow == NULL) {
        // If all windows have been closed, exit the demo.
        exit(0);
    }
}

static void composit(struct Rectangle rect) {
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
    draw.lfb_pitch = displayRect.width * 4;
    draw.lfb_x = 0;
    draw.lfb_y = 0;
    draw.draw_x = rect.x;
    draw.draw_y = rect.y;
    draw.draw_width = rect.width;
    draw.draw_height = rect.height;
    posix_devctl(displayFd, DISPLAY_DRAW, &draw, sizeof(draw), NULL);
}

static void eventLoop(void) {
    damageRect.width = 0;

    struct pollfd pfd[1];
    pfd[0].fd = mouseFd;
    pfd[0].events = POLLIN;

    int oldX = mouseX;
    int oldY = mouseY;
    struct Rectangle oldMouseRect = getMouseRect();

    while (poll(pfd, 1, 0) >= 1) {
        if (pfd[0].revents & POLLIN) {
            handleMouse();
        }
    }

    if (oldX != mouseX || oldY != mouseY) {
        addDamageRect(oldMouseRect);
        addDamageRect(getMouseRect());
    }

    if (damageRect.width != 0) {
        composit(damageRect);
    }
}

static struct Rectangle getClientRect(struct Window* window) {
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

static struct Rectangle getMouseRect(void) {
    struct Rectangle result;
    result.x = mouseX - cursorSize / 2;
    result.y = mouseY - cursorSize / 2;
    result.width = cursorSize;
    result.height = cursorSize;
    return result;
}

static void handleMouse(void) {
    struct mouse_data data;
    read(mouseFd, &data, sizeof(data));

    int oldX = mouseX;
    int oldY = mouseY;

    mouseX += data.mouse_x;
    mouseY += data.mouse_y;
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

    if (!leftClick && data.mouse_flags & MOUSE_LEFT) {
        leftClick = true;
        struct Window* window;
        int status = checkMouseInteraction(&window);
        if (window) {
            moveWindowToTop(window);
            if (status == CLIENT_AREA) {
                // The mouse click needs to be handled by the client.
            } else if (status == CLOSE_BUTTON) {
                closeWindow(window);
            } else if (status == TITLE_BAR) {
                changingWindow = window;
            } else {
                changingWindow = window;
                resizeDirection = status;
            }
        }
    } else if (leftClick && !(data.mouse_flags & MOUSE_LEFT)) {
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

        resizeWindow(changingWindow, rect);
    } else if (!leftClick) {
        struct Window* window;
        int status = checkMouseInteraction(&window);
        uint32_t* newCursor;
        if (status == RESIZE_LEFT || status == RESIZE_RIGHT) {
            newCursor = resizeHCursor;
        } else if (status == RESIZE_TOP || status == RESIZE_BOTTOM) {
            newCursor = resizeVCursor;
        } else if (status == RESIZE_TOP_LEFT || status == RESIZE_BOTTOM_RIGHT) {
            newCursor = resizeD1Cursor;
        } else if (status == RESIZE_TOP_RIGHT || status == RESIZE_BOTTOM_LEFT) {
            newCursor = resizeD2Cursor;
        } else {
            newCursor = arrowCursor;
        }

        if (cursor != newCursor) {
            cursor = newCursor;
            addDamageRect(getMouseRect());
        }
    }
}

static void initialize(void) {
    loadAssets();
    cursor = arrowCursor;

    displayFd = open("/dev/display", O_RDONLY);
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
    signal(SIGQUIT, SIG_IGN);
    signal(SIGSEGV, onSignal);
    signal(SIGTERM, onSignal);

    mode = DISPLAY_MODE_LFB;
    errno = posix_devctl(displayFd, DISPLAY_SET_MODE, &mode, sizeof(mode),
            NULL);
    if (errno) err(1, "cannot set display mode");

    mouseFd = open("/dev/mouse", O_RDONLY);
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

    struct display_resolution res;
    posix_devctl(displayFd, DISPLAY_GET_RESOLUTION, &res, sizeof(res), NULL);
    displayRect.x = 0;
    displayRect.y = 0;
    displayRect.width = res.width;
    displayRect.height = res.height;
    mouseX = displayRect.width / 2;
    mouseY = displayRect.height / 2;

    lfb = malloc(displayRect.width * 4 * displayRect.height);
    if (!lfb) err(1, "malloc");

    // Create some windows.
    struct Rectangle rect1 = {250, 50, 200, 300};
    addWindow(rect1, RGBA(255, 0, 100, 220), "First window");
    struct Rectangle rect2 = {100, 200, 300, 300};
    addWindow(rect2, RGB(230, 230, 230), "Second window");
    struct Rectangle rect3 = {350, 150, 300, 300};
    addWindow(rect3, RGBA(60, 255, 60, 200), "Third window");
    struct Rectangle rect4 = {300, 400, 400, 200};
    addWindow(rect4, RGB(255, 255, 255), "Fourth window");
}

static bool isInRect(int x, int y, struct Rectangle rect) {
    return x >= rect.x && x < rect.x + rect.width && y >= rect.y &&
            y < rect.y + rect.height;
}

static void loadAssets(void) {
    loadFromFile("/share/fonts/vgafont", vgafont, sizeof(vgafont));
    loadFromFile("/share/cursors/arrow.rgba", arrowCursor, sizeof(arrowCursor));
    loadFromFile("/share/cursors/resize_diagonal1.rgba", resizeD1Cursor,
            sizeof(resizeD1Cursor));
    loadFromFile("/share/cursors/resize_diagonal2.rgba", resizeD2Cursor,
            sizeof(resizeD2Cursor));
    loadFromFile("/share/cursors/resize_horizontal.rgba", resizeHCursor,
            sizeof(resizeHCursor));
    loadFromFile("/share/cursors/resize_vertical.rgba", resizeVCursor,
            sizeof(resizeVCursor));
}

static void loadFromFile(const char* filename, void* buffer, size_t size) {
    FILE* file = fopen(filename, "r");
    if (!file) err(1, "cannot open '%s'", filename);
    size_t bytesRead = fread(buffer, 1, size, file);
    if (bytesRead < size) errx(1, "cannot load '%s", filename);
    fclose(file);
}

static void moveWindowToTop(struct Window* window) {
    if (window == topWindow) return;
    removeWindow(window);
    addWindowOnTop(window);
    addDamageRect(window->rect);
}

static void onSignal(int signo) {
    restoreDisplay();
    signal(signo, SIG_DFL);
    raise(signo);
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

static uint32_t renderCloseButton(int x, int y) {
    if (((x == y) || (y == windowCloseButtonSize - 1 - x)) && x > 2 && x < 13) {
        return closeCrossColor;
    } else {
        return closeButtonColor;
    }
}

static uint32_t renderPixel(int x, int y) {
    uint32_t rgba = 0;
    if (isInRect(x, y, getMouseRect())) {
        int xPixel = x - (mouseX - cursorSize / 2);
        int yPixel = y - (mouseY - cursorSize / 2);
        rgba = cursor[yPixel * cursorSize + xPixel];
    }

    for (struct Window* window = topWindow; window; window = window->below) {
        if (!isInRect(x, y, window->rect)) continue;
        struct Rectangle clientRect = getClientRect(window);

        uint32_t color;
        if (isInRect(x, y, clientRect)) {
            color = window->color;
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

static uint32_t renderWindowDecoration(struct Window* window, int x, int y) {
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
        const char* font = &vgafont[window->title[i] * fontHeight];
        size_t j = y - windowBorderSize;
        size_t k = (x - titleBegin) % fontWidth;
        if (k != 8 && font[j] & (1 << (7 - k))) {
            return titleColor;
        } else {
            return windowDecorationColor;
        }
    }
}

static void resizeWindow(struct Window* window, struct Rectangle rect) {
    if (rect.width < minimumWindowWidth || rect.height < minimumWindowHeight) {
        return;
    }
    addDamageRect(window->rect);
    window->rect = rect;
    addDamageRect(rect);
}

static void restoreDisplay(void) {
    posix_devctl(displayFd, DISPLAY_SET_MODE, &oldMode, sizeof(oldMode), NULL);
}

int main(void) {
    initialize();
    composit(displayRect);
    while (true) {
        eventLoop();
    }
}
