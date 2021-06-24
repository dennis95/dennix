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

/* gui/gui.h
 * Graphical User Interface.
 */

#ifndef GUI_H
#define GUI_H

#include <dxui.h>

extern dxui_context* context;
extern dxui_window* compositorWindow;
extern dxui_color* lfb;
extern dxui_dim guiDim;

static const dxui_color backgroundColor = RGB(0, 200, 255);

void addDamageRect(dxui_rect rect);
void broadcastStatusEvent(void);
void composit(void);
void handleKey(dxui_control* control, dxui_key_event* event);
void handleMouse(dxui_control* control, dxui_mouse_event* event);
void handleResize(dxui_window* window, dxui_resize_event* event);
void initializeDisplay(void);
void initializeServer(void);
void pollEvents(void);

#endif
