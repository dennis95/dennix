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

/* libdxui/src/button.
 * Button control.
 */

#include "control.h"
#include <stdlib.h>
#include <string.h>

typedef struct dxui_internal_button {
    union {
        Control control;
        dxui_control* dxui_as_control;
    };
} Button;

static void deleteButton(Control* control);
static void redrawButton(Control* control, dxui_dim dim, dxui_color* lfb,
        unsigned int pitch);

static const ControlClass buttonClass = {
    .delete = deleteButton,
    .redraw = redrawButton,
};

dxui_button* dxui_create_button(dxui_rect rect, const char* text) {
    Button* button = calloc(1, sizeof(Button));
    if (!button) return NULL;
    button->control.self = button;
    button->control.class = &buttonClass;
    button->control.rect = rect;
    button->control.background = COLOR_WHITE_SMOKE;
    button->control.text = strdup(text);
    if (!button->control.text) {
        free(button);
        return NULL;
    }

    return (dxui_button*) button;
}

static void deleteButton(Control* control) {
    (void) control;
}

static void redrawButton(Control* control, dxui_dim dim, dxui_color* lfb,
        unsigned int pitch) {
    static const int margin = 2;
    dxui_rect rect = dxui_rect_crop(control->rect, dim);

    for (int y = 0; y < rect.height; y++) {
        for (int x = 0; x < rect.width; x++) {
            size_t index = (rect.y + y) * pitch + rect.x + x;

            if ((x <= margin && y < control->rect.height - x) ||
                    (y <= margin && x < control->rect.width - y)) {
                lfb[index] = COLOR_SILVER;
            } else if (x >= control->rect.width - 1 - margin ||
                    y >= control->rect.height - 1 - margin) {
                lfb[index] = COLOR_GRAY;
            } else {
                lfb[index] = control->background;
            }
        }
    }

    dxui_context* context = control->owner->class->getContext(control->owner);
    dxui_draw_text(context, lfb, control->text, COLOR_BLACK,
            control->rect, rect, pitch, DXUI_TEXT_CENTERED);

    control->owner->class->invalidate(control->owner, control->rect);
}
