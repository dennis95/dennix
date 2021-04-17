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

/* libdxui/src/control.
 * Controls.
 */

#ifndef CONTROL_H
#define CONTROL_H

#include <dxui.h>

typedef struct dxui_internal_control Control;
typedef struct dxui_internal_container Container;

typedef struct {
    void (*delete)(Control*);
    void (*redraw)(Control*, dxui_dim, dxui_color*, unsigned int);
} ControlClass;

extern const ControlClass dxui_windowControlClass;

typedef struct {
    dxui_context* (*getContext)(Container*);
    dxui_color* (*getFramebuffer)(Container*, dxui_dim*, unsigned int*);
    void (*invalidate)(Container*, dxui_rect);
} ContainerClass;

struct dxui_internal_control {
    union {
        void* self;
        dxui_control* dxui_as_control;
    };
    const ControlClass* class;
    Container* owner;
    Control* prev;
    Control* next;
    void* eventHandlers[DXUI_EVENT_NUM];
    void* userData;
    char* text;
    dxui_rect rect;
    dxui_color background;
};

struct dxui_internal_container {
    union {
        Control control;
        dxui_control* dxui_as_control;
        dxui_container* dxui_as_container;
    };
    const ContainerClass* class;
    Control* firstControl;
};

#endif
