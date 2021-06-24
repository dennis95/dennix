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

/* gui/keyboard.c
 * Keyboard input.
 */

#include <dennix/kbkeys.h>
#include <stdlib.h>
#include <unistd.h>
#include "connection.h"
#include "window.h"

static bool leftGuiKey;
static bool rightGuiKey;

void handleKey(dxui_control* control, dxui_key_event* event) {
    (void) control;
    struct Window* window = topWindow;

    if (event->key == KB_LGUI) {
        leftGuiKey = true;
    } else if (event->key == -KB_LGUI) {
        leftGuiKey = false;
    } else if (event->key == KB_RGUI) {
        rightGuiKey = true;
    } else if (event->key == -KB_RGUI) {
        rightGuiKey = false;
    }

    bool guiKey = leftGuiKey || rightGuiKey;

    if (guiKey && !(window && window->flags & GUI_WINDOW_COMPOSITOR)) {
        if (event->key == KB_T) {
            pid_t pid = fork();
            if (pid == 0) {
                execl("/bin/terminal", "terminal", NULL);
                _Exit(1);
            }
        } else if (event->key == KB_Q) {
            for (struct Window* win = topWindow; win; win = win->below) {
                closeWindow(win);
            }
            exit(0);
        }
    } else if (window) {
        struct gui_event_key guiEvent;
        guiEvent.window_id = window->id;
        guiEvent.key = event->key;
        guiEvent.codepoint = event->codepoint;
        sendEvent(window->connection, GUI_EVENT_KEY, sizeof(guiEvent),
                &guiEvent);
    }
}
