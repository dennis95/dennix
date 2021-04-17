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

/* libdxui/src/label.c
 * Label control.
 */

#include "control.h"
#include <stdlib.h>
#include <string.h>

typedef struct dxui_internal_label {
    union {
        Control control;
        dxui_control* dxui_as_control;
    };
} Label;

static void deleteLabel(Control* control);
static void redrawLabel(Control* control, dxui_dim dim, dxui_color* lfb,
        unsigned int pitch);

static const ControlClass labelClass = {
    .delete = deleteLabel,
    .redraw = redrawLabel,
};

dxui_label* dxui_create_label(dxui_rect rect, const char* text) {
    Label* label = calloc(1, sizeof(Label));
    if (!label) return NULL;
    label->control.self = label;
    label->control.class = &labelClass;
    label->control.rect = rect;
    label->control.background = COLOR_WHITE_SMOKE;
    label->control.text = strdup(text);
    if (!label->control.text) {
        free(label);
        return NULL;
    }

    return (dxui_label*) label;
}

static void deleteLabel(Control* control) {
    (void) control;
}

static void redrawLabel(Control* control, dxui_dim dim, dxui_color* lfb,
        unsigned int pitch) {
    dxui_rect rect = dxui_rect_crop(control->rect, dim);

    for (int y = 0; y < rect.height; y++) {
        for (int x = 0; x < rect.width; x++) {
            size_t index = (rect.y + y) * pitch + rect.x + x;
            lfb[index] = control->background;
        }
    }

    dxui_context* context = control->owner->class->getContext(control->owner);
    dxui_draw_text(context, lfb, control->text, COLOR_BLACK, control->rect,
            rect, pitch, 0);
    control->owner->class->invalidate(control->owner, control->rect);
}
