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

/* libdxui/src/window.c
 * Windows.
 */

#include <dxui.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/guimsg.h>
#include "context.h"

static void deleteWindow(Control* control);
static void redrawWindow(Control* control, dxui_dim dim, dxui_color* lfb,
        unsigned int pitch);
static dxui_context* getWindowContext(Container* container);
static dxui_color* getWindowFramebuffer(Container* container, dxui_dim* dim,
        unsigned int* pitch);
static void updateRect(Window* window, dxui_rect rect);
static void invalidateWindowRect(Container* container, dxui_rect rect);

const ControlClass dxui_windowControlClass = {
    .delete = deleteWindow,
    .redraw = redrawWindow,
};
static const ContainerClass windowContainerClass = {
    .getContext = getWindowContext,
    .getFramebuffer = getWindowFramebuffer,
    .invalidate = invalidateWindowRect,
};

void dxui_close(dxui_window* window) {
    Window* win = window->internal;
    dxui_context* context = win->context;

    context->backend->closeWindow(context, win->id);

    void (*handler)(dxui_window*) =
            win->control.eventHandlers[DXUI_EVENT_WINDOW_CLOSE];
    if (handler) {
        handler(window);
    }

    Control* control = win->container.firstControl;
    while (control) {
        Control* next = control->next;
        dxui_delete(control);
        control = next;
    }

    if (win->prev) {
        win->prev->next = win->next;
    } else {
        context->firstWindow = win->next;
    }

    if (win->next) {
        win->next->prev = win->prev;
    }

    free(win->lfb);
    free(window);
}

dxui_window* dxui_create_window(dxui_context* context, dxui_rect rect,
        const char* title, int flags) {
    Window* window = calloc(1, sizeof(Window));
    if (!window) return NULL;
    window->control.self = window;
    window->control.class = &dxui_windowControlClass;
    window->control.text = strdup(title);
    if (!window->control.text) {
        free(window);
        return NULL;
    }
    window->compositorTitle = window->control.text;
    window->control.rect = rect;
    window->control.background = COLOR_WHITE_SMOKE;
    window->compositorBackground = COLOR_WHITE_SMOKE;
    window->container.class = &windowContainerClass;
    window->context = context;
    window->idAssigned = false;

    window->lfbDim = rect.dim;
    window->lfb = malloc(rect.width * rect.height * sizeof(dxui_color));
    if (!window->lfb) {
        free(window->control.text);
        free(window);
        return NULL;
    }
    window->redraw = true;

    window->next = context->firstWindow;
    if (window->next) {
        window->next->prev = window;
    }
    context->firstWindow = window;

    context->backend->createWindow(context, rect, title, flags);

    while (!window->idAssigned) {
        dxui_pump_events(context, DXUI_PUMP_ONCE, -1);
    }

    dxui_update(window);
    return DXUI_AS_WINDOW(window);
}

dxui_color* dxui_get_framebuffer(dxui_window* window, dxui_dim dim) {
    Window* win = window->internal;
    if (dim.width != win->lfbDim.width || dim.height != win->lfbDim.height) {
        dxui_color* lfb = malloc(dim.height * dim.width * sizeof(dxui_color));
        if (!lfb) return NULL;
        free(win->lfb);
        win->lfb = lfb;
        win->lfbDim = dim;
    }

    win->manualDrawing = true;
    win->redraw = true;
    return win->lfb;
}

void dxui_hide(dxui_window* window) {
    Window* win = window->internal;
    win->visible = false;
    win->context->backend->hideWindow(win->context, win->id);
}

void dxui_release_framebuffer(dxui_window* window) {
    Window* win = window->internal;
    win->manualDrawing = false;
    win->redraw = true;
    dxui_update(window);
}

void dxui_resize_window(dxui_window* window, dxui_dim dim) {
    Window* win = window->internal;
    win->control.rect.dim = dim;
    win->context->backend->resizeWindow(win->context, win->id, dim);
}

void dxui_set_cursor(dxui_window* window, int cursor) {
    Window* win = window->internal;
    win->context->backend->setWindowCursor(win->context, win->id, cursor);
}

void dxui_set_relative_mouse(dxui_window* window, bool relative) {
    Window* win = window->internal;
    win->relativeMouse = relative;
    win->context->backend->setRelativeMouse(win->context, win->id, relative);
}

void dxui_show(dxui_window* window) {
    Window* win = window->internal;
    win->visible = true;
    win->context->backend->showWindow(win->context, win->id);
}

void dxui_update_framebuffer(dxui_window* window, dxui_rect rect) {
    Window* win = window->internal;
    rect = dxui_rect_crop(rect, win->lfbDim);
    if (win->redraw) {
        win->context->backend->redrawWindow(win->context, win->id, win->lfbDim,
                win->lfb);
        win->redraw = false;
    } else {
        updateRect(win, rect);
    }
}

static void deleteWindow(Control* control) {
    Window* window = (Window*) control;
    dxui_close(DXUI_AS_WINDOW(window));
}

static void redrawWindow(Control* control, dxui_dim dim, dxui_color* lfb,
        unsigned int pitch) {
    Window* window = (Window*) control;
    if (window->manualDrawing) {
        window->context->backend->redrawWindow(window->context, window->id,
                window->lfbDim, window->lfb);
        window->redraw = false;
        return;
    }
    window->updateInProgress = true;

    // Inform the compositor about changed backgrounds and titles.
    if (window->compositorBackground != control->background) {
        window->context->backend->setWindowBackground(window->context,
                window->id, control->background);
        window->compositorBackground = control->background;
    }

    if (window->compositorTitle != control->text) {
        window->context->backend->setWindowTitle(window->context, window->id,
                control->text);
        window->compositorTitle = control->text;
    }

    for (int y = 0; y < dim.height; y++) {
        for (int x = 0; x < dim.width; x++) {
            lfb[y * pitch + x] = control->background;
        }
    }

    control = window->container.firstControl;
    while (control) {
        control->class->redraw(control, dim, lfb, pitch);
        control = control->next;
    }
    window->updateInProgress = false;

    window->context->backend->redrawWindow(window->context, window->id,
            window->lfbDim, window->lfb);

    window->redraw = false;
}

static dxui_context* getWindowContext(Container* container) {
    Window* window = (Window*) container;
    return window->context;
}

static dxui_color* getWindowFramebuffer(Container* container, dxui_dim* dim,
        unsigned int* pitch) {
    Window* window = (Window*) container;
    *dim = window->lfbDim;
    *pitch = window->lfbDim.width;
    return window->lfb;
}

static void updateRect(Window* window, dxui_rect rect) {
    window->context->backend->redrawWindowPart(window->context, window->id,
            window->lfbDim.width, rect, window->lfb);
}

static void invalidateWindowRect(Container* container, dxui_rect rect) {
    Window* window = (Window*) container;
    if (window->updateInProgress) return;
    updateRect(window, dxui_rect_crop(rect, window->lfbDim));
}
