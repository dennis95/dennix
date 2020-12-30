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

/* gui/keyboard.c
 * Keyboard input.
 */

#include <err.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <dennix/kbkeys.h>

#include "connection.h"
#include "keyboard.h"
#include "window.h"

static struct termios oldTermios;
static char partialKeyBuffer[sizeof(struct kbwc)];
static size_t partialKeyBytes;

static void restoreTermios(void);

void handleKeyboard(void) {
    struct kbwc kbwc[1024];

    for (size_t i = 0; i < partialKeyBytes; i++) {
        ((char*) kbwc)[i] = partialKeyBuffer[i];
    }

    ssize_t bytes = read(0, (char*) kbwc + partialKeyBytes,
            sizeof(kbwc) - partialKeyBytes);
    if (bytes < 0) err(1, "read");
    bytes += partialKeyBytes;

    struct Window* window = topWindow;

    size_t numKeys = bytes / sizeof(struct kbwc);
    for (size_t i = 0; i < numKeys; i++) {
        if (window) {
            struct gui_event_key event;
            event.window_id = window->id;
            event.key = kbwc[i].kb;
            event.codepoint = kbwc[i].wc;
            sendEvent(window->connection, GUI_EVENT_KEY, sizeof(event), &event);
        }
    }

    partialKeyBytes = bytes % sizeof(struct kbwc);
    for (size_t i = 0; i < partialKeyBytes; i++) {
        partialKeyBuffer[i] = *(char*) kbwc + bytes - partialKeyBytes + i;
    }
}

void initializeKeyboard(void) {
    if (!isatty(0)) err(1, "stdin is not a terminal");

    tcgetattr(0, &oldTermios);
    atexit(restoreTermios);
    struct termios newTermios = oldTermios;
    newTermios.c_lflag |= _KBWC;
    tcsetattr(0, TCSAFLUSH, &newTermios);
}

static void restoreTermios(void) {
    tcsetattr(0, TCSAFLUSH, &oldTermios);
}
