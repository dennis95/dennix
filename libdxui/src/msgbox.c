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

/* libdxui/src/msgbox.
 * Message Box.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "context.h"

static void onMsgBoxCloseButton(dxui_window* window) {
    (void) window;
}

static void onMsgBoxButton(dxui_control* control, dxui_mouse_event* event) {
    (void) event;

    int* result = dxui_get_user_data(control->internal->owner);
    const char* text = control->internal->text;
    if (strcmp(text, "OK") == 0) {
        *result = DXUI_MSG_BOX_OK;
    } else if (strcmp(text, "Yes") == 0) {
        *result = DXUI_MSG_BOX_YES;
    } else if (strcmp(text, "No") == 0) {
        *result = DXUI_MSG_BOX_NO;
    } else if (strcmp(text, "Cancel") == 0) {
        *result = DXUI_MSG_BOX_CANCEL;
    } else {
        *result = -1;
    }
    dxui_close((dxui_window*) control->internal->owner);
}

int dxui_msg_box(dxui_context* context, const char* title, const char* text,
        int flags) {
    size_t numButtons = 0;
    if (flags & DXUI_MSG_BOX_OK) numButtons++;
    if (flags & DXUI_MSG_BOX_YES) numButtons++;
    if (flags & DXUI_MSG_BOX_NO) numButtons++;
    if (flags & DXUI_MSG_BOX_CANCEL) numButtons++;

    if (numButtons == 0) {
        flags |= DXUI_MSG_BOX_OK;
        numButtons = 1;
    }

    dxui_rect rect = {{ 10, 10, numButtons * 110, 16 }};
    dxui_rect textRect = dxui_get_text_rect(text, rect, DXUI_TEXT_CENTERED);

    if (textRect.x < 10) {
        textRect.x = 10;
    }

    rect.width = textRect.width + 2 * textRect.x;
    rect.height = 100;
    rect.x = context->displayDim.width / 2 - rect.width / 2;
    rect.y = context->displayDim.height / 2 - rect.height / 2;

    dxui_window* window = dxui_create_window(context, rect, title,
            DXUI_WINDOW_NO_RESIZE);
    if (!window) return -1;

    dxui_label* label = dxui_create_label(textRect, text);
    if (!label) {
        dxui_delete(window);
        return -1;
    }
    dxui_add_control(window, label);

    dxui_rect buttonRect = {{ (rect.width - numButtons * 110 - 10) / 2 + 10,
            70, 100, 20 }};
    const char* buttonText[] = { "OK", "Yes", "No", "Cancel" };

    for (size_t i = 0; i < 4; i++) {
        if (!(flags & (1 << i))) continue;
        dxui_button* button = dxui_create_button(buttonRect, buttonText[i]);
        if (!button) {
            dxui_delete(window);
            return -1;
        }
        dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onMsgBoxButton);
        dxui_add_control(window, button);
        buttonRect.x += 110;
    }

    int result = 0;
    dxui_set_user_data(window, &result);
    dxui_set_event_handler(window, DXUI_EVENT_WINDOW_CLOSE_BUTTON,
            onMsgBoxCloseButton);
    dxui_show(window);

    while (!result) {
        dxui_pump_events(context, DXUI_PUMP_ONCE, -1);
    }

    return result;
}

void dxui_show_message(dxui_context* context, const char* text) {
    if (context) {
        if (dxui_msg_box(context, program_invocation_short_name, text,
                DXUI_MSG_BOX_OK) != -1) return;
    }

    fprintf(stderr, "%s: %s\n", program_invocation_short_name, text);
}

void dxui_panic(dxui_context* context, const char* text) {
    dxui_show_message(context, text);
    exit(1);
}
