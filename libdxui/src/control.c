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

/* libdxui/src/control.c
 * Controls.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "control.h"

void (dxui_delete)(dxui_control* control) {
    Control* internal = control->internal;
    if (internal->owner) {
        if (internal->prev) {
            internal->prev->next = internal->next;
        } else {
            internal->owner->firstControl = internal->next;
        }

        if (internal->next) {
            internal->next->prev = internal->prev;
        }

        dxui_update(internal->owner);
    }

    internal->class->delete(internal);

    free(internal->text);
    free(control);
}

dxui_dim (dxui_get_dim)(dxui_control* control) {
    return control->internal->rect.dim;
}

const char* (dxui_get_text)(dxui_control* control) {
    return control->internal->text;
}

void* (dxui_get_user_data)(dxui_control* control) {
    return control->internal->userData;
}

void (dxui_set_background)(dxui_control* control, dxui_color background) {
    control->internal->background = background;
    dxui_update(control);
}

bool (dxui_set_text)(dxui_control* control, const char* text) {
    char* newText = strdup(text);
    if (!newText) return false;

    free(control->internal->text);
    control->internal->text = newText;
    dxui_update(control);
    return true;
}

bool (dxui_set_text_format)(dxui_control* control, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    char* text;
    if (vasprintf(&text, format, ap) < 0) {
        va_end(ap);
        return false;
    }
    va_end(ap);

    free(control->internal->text);
    control->internal->text = text;
    dxui_update(control);
    return true;
}

void (dxui_set_user_data)(dxui_control* control, void* data) {
    control->internal->userData = data;
}

void (dxui_update)(dxui_control* control) {
    Container* owner = control->internal->owner;
    if (!owner && control->internal->class == &dxui_windowControlClass) {
        owner = (Container*) control;
    }

    if (!owner) return;

    dxui_dim dim;
    unsigned int pitch;
    dxui_color* framebuffer = owner->class->getFramebuffer(owner, &dim, &pitch);
    if (!framebuffer) return;

    control->internal->class->redraw(control->internal, dim, framebuffer,
            pitch);
}

void (dxui_add_control)(dxui_container* container, dxui_control* control) {
    Control* internal = control->internal;
    internal->next = container->internal->firstControl;
    if (internal->next) {
        internal->next->prev = internal;
    }
    container->internal->firstControl = internal;
    internal->owner = container->internal;
    dxui_update(control);
}

dxui_control* (dxui_get_control_at)(dxui_container* container, dxui_pos pos) {
    Control* control = container->internal->firstControl;
    while (control) {
        if (dxui_rect_contains_pos(control->rect, pos)) {
            return (dxui_control*) control;
        }
        control = control->next;
    }
    return DXUI_AS_CONTROL(container);
}
