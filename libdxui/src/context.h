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

/* libdxui/src/context.h
 * dxui context.
 */

#ifndef CONTEXT_H
#define CONTEXT_H

#include <dennix/kbkeys.h>
#include "window.h"

typedef struct {
    void (*closeWindow)(dxui_context* context, unsigned int id);
    void (*createWindow)(dxui_context* context, dxui_rect rect,
            const char* title, int flags);
    void (*hideWindow)(dxui_context* context, unsigned int id);
    void (*resizeWindow)(dxui_context* context, unsigned int id, dxui_dim dim);
    void (*setWindowCursor)(dxui_context* context, unsigned int id, int cursor);
    void (*showWindow)(dxui_context* context, unsigned int id);
    void (*setRelativeMouse)(dxui_context* context, unsigned int id,
            bool relative);
    void (*setWindowBackground)(dxui_context* context, unsigned int id,
            dxui_color color);
    void (*setWindowTitle)(dxui_context* context, unsigned int id,
            const char* title);
    void (*redrawWindow)(dxui_context* context, unsigned int id, dxui_dim dim,
                dxui_color* lfb);
    void (*redrawWindowPart)(dxui_context* context, unsigned int id,
            unsigned int pitch, dxui_rect rect, dxui_color* lfb);
} Backend;

extern const Backend dxui_compositorBackend;
extern const Backend dxui_standaloneBackend;

struct dxui_context {
    const Backend* backend;
    Window* firstWindow;
    dxui_dim displayDim;
    dxui_pos mouseDownPos;
    bool mouseDown;
    char vgafont[4096];

    // Used by the compositor backend:
    int socket;

    // Used by the standalone backend:
    int displayFd;
    int mouseFd;
    dxui_color* cursors;
    dxui_color* framebuffer;
    Window* activeWindow;
    char partialKeyBuffer[sizeof(struct kbwc) - 1];
    size_t partialKeyBytes;
    dxui_pos mousePos;
    dxui_pos viewport;
    int idCounter;
};

#endif
