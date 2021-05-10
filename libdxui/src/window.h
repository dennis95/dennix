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

/* libdxui/src/window.h
 * Windows.
 */

#ifndef WINDOW_H
#define WINDOW_H

#include "control.h"

typedef struct dxui_internal_window Window;
struct dxui_internal_window {
    union {
        Control control;
        Container container;
        dxui_control* dxui_as_control;
        dxui_container* dxui_as_container;
        dxui_window* dxui_as_window;
    };
    dxui_context* context;
    Window* prev;
    Window* next;
    unsigned int id;
    bool idAssigned;
    dxui_dim lfbDim;
    uint32_t* lfb;
    bool redraw;
    bool updateInProgress;
    bool visible;
    dxui_color compositorBackground;
    const char* compositorTitle;
};

#define DXUI_AS_WINDOW(window) ((window)->dxui_as_window)

#endif
