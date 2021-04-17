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

/* gui/gui-test.c
 * GUI test program.
 */

#include <dxui.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static dxui_context* context;

static void addWindow(void);
static void newWindowClick(dxui_control* control, dxui_mouse_event* event);
static void newClientClick(dxui_control* control, dxui_mouse_event* event);
static void messageBoxClick(dxui_control* control, dxui_mouse_event* event);
static void onKey(dxui_window* window, dxui_key_event* event);
static void shutdown(void);

static void addWindow(void) {
    dxui_rect rect = {{ -1, -1, 500, 350 }};
    dxui_window* window = dxui_create_window(context, rect, "Hello World", 0);
    if (!window) dxui_panic(context, "Failed to create a window");

    rect = (dxui_rect) {{ 50, 50, 150, 30 }};
    dxui_button* button = dxui_create_button(rect, "New Window");
    if (!button) dxui_panic(context, "Failed to create a button");
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, newWindowClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 50, 100, 150, 30 }};
    button = dxui_create_button(rect, "New Client");
    if (!button) dxui_panic(context, "Failed to create a button");
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, newClientClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 50, 150, 150, 30 }};
    button = dxui_create_button(rect, "Show Message Box");
    if (!button) dxui_panic(context, "Failed to create a button");
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, messageBoxClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 50, 200, 150, 30 }};
    dxui_label* label = dxui_create_label(rect, "");
    if (!label) dxui_panic(context, "Failed to create a label");
    dxui_set_background(label, COLOR_WHITE);
    dxui_add_control(window, label);
    dxui_set_user_data(window, label);

    dxui_set_event_handler(window, DXUI_EVENT_KEY, onKey);

    dxui_show(window);
}

static void newWindowClick(dxui_control* control, dxui_mouse_event* event) {
    (void) control; (void) event;
    addWindow();
}

static void newClientClick(dxui_control* control, dxui_mouse_event* event) {
    (void) control; (void) event;
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/gui-test", "gui-test", NULL);
        _Exit(1);
    }
}

static void messageBoxClick(dxui_control* control, dxui_mouse_event* event) {
    (void) control; (void) event;
    dxui_msg_box(context, "Message", "Hello World", DXUI_MSG_BOX_OK);
}

static void onKey(dxui_window* window, dxui_key_event* event) {
    if (event->codepoint) {
        dxui_label* label = dxui_get_user_data(window);
        const char* text = dxui_get_text(label);
        size_t length = strlen(text);
        if (length > 15) {
            text += length - 15;
        }
        dxui_set_text_format(label, "%s%c", text, (char) event->codepoint);
    }
}

static void shutdown(void) {
    dxui_shutdown(context);
}

int main() {
    atexit(shutdown);
    context = dxui_initialize(0);
    if (!context) dxui_panic(NULL, "Failed to initialize dxui");

    addWindow();
    dxui_pump_events(context, DXUI_PUMP_WHILE_WINDOWS_EXIST);
}
