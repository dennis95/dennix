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

/* libdxui/include/dxui.h
 * The Dennix User Interface library.
 */

#ifndef _DXUI_H
#define _DXUI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/colors.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Basic types. */

typedef struct {
    int x;
    int y;
} dxui_pos;

typedef struct {
    int width;
    int height;
} dxui_dim;

typedef union {
    struct {
        int x;
        int y;
        int width;
        int height;
    };
    struct {
        dxui_pos pos;
        dxui_dim dim;
    };
} dxui_rect;

typedef uint32_t dxui_color;

/* The dxui context. */

typedef struct dxui_context dxui_context;

dxui_dim dxui_get_display_dim(dxui_context* /*context*/);

enum {
    /* Fail if no compositor is available. */
    DXUI_INIT_NEED_COMPOSITOR = 1 << 0,
    /* Make sure a mouse cursor is available even when running without
       compositor. */
    DXUI_INIT_CURSOR = 1 << 1,
};

/* Initialize the dxui context. */
dxui_context* dxui_initialize(int /*flags*/);

bool dxui_is_standalone(dxui_context* /*context*/);

/* Shutdown the dxui context. Must be called on exit. */
void dxui_shutdown(dxui_context* /*context*/);

/* Controls. */

typedef union dxui_control {
    union dxui_control* dxui_as_control;
    struct dxui_internal_control* dxui_internal;
} dxui_control;

#define DXUI_AS_CONTROL(control) ((control)->dxui_as_control)

void dxui_delete(dxui_control* /*control*/);
#define dxui_delete(control) dxui_delete(DXUI_AS_CONTROL(control))

dxui_dim dxui_get_dim(dxui_control* /*control*/);
#define dxui_get_dim(control) dxui_get_dim(DXUI_AS_CONTROL(control))

const char* dxui_get_text(dxui_control* /*control*/);
#define dxui_get_text(control) dxui_get_text(DXUI_AS_CONTROL(control))

void* dxui_get_user_data(dxui_control* /*control*/);
#define dxui_get_user_data(control) dxui_get_user_data(DXUI_AS_CONTROL(control))

void dxui_set_background(dxui_control* /*control*/, dxui_color /*background*/);
#define dxui_set_background(control, background) \
        dxui_set_background(DXUI_AS_CONTROL(control), background)

bool dxui_set_text(dxui_control* /*control*/, const char* /*text*/);
#define dxui_set_text(control, text) \
        dxui_set_text(DXUI_AS_CONTROL(control), text)

bool dxui_set_text_format(dxui_control* /*control*/, const char* /*format*/,
        ...);
#define dxui_set_text_format(control, ...) \
        dxui_set_text_format(DXUI_AS_CONTROL(control), __VA_ARGS__)

void dxui_set_user_data(dxui_control* /*control*/, void* /*data*/);
#define dxui_set_user_data(control, data) \
        dxui_set_user_data(DXUI_AS_CONTROL(control), data)

void dxui_update(dxui_control* /*control*/);
#define dxui_update(control) dxui_update(DXUI_AS_CONTROL(control))

/* Event handling. */

enum {
    DXUI_EVENT_KEY,
    DXUI_EVENT_MOUSE,
    DXUI_EVENT_MOUSE_CLICK,
    DXUI_EVENT_MOUSE_DOWN,
    DXUI_EVENT_MOUSE_UP,
    DXUI_EVENT_WINDOW_CLOSE,
    DXUI_EVENT_WINDOW_CLOSE_BUTTON,
    DXUI_EVENT_WINDOW_RESIZED,

    DXUI_EVENT_NUM
};

enum {
    DXUI_MOUSE_LEFT = 1 << 0,
    DXUI_MOUSE_RIGHT = 1 << 1,
    DXUI_MOUSE_MIDDLE = 1 << 2,
    DXUI_MOUSE_SCROLL_UP = 1 << 3,
    DXUI_MOUSE_SCROLL_DOWN = 1 << 4,
    DXUI_MOUSE_LEAVE = 1 << 5,
};

typedef struct {
    int key;
    wchar_t codepoint;
} dxui_key_event;

typedef struct {
    dxui_pos pos;
    int flags;
} dxui_mouse_event;

typedef struct {
    dxui_dim dim;
} dxui_resize_event;

enum {
    /* Block until one event has been processed. */
    DXUI_PUMP_ONCE,
    /* Pump until all pending events have been processed. */
    DXUI_PUMP_CLEAR,
    /* Block until one event has been processed and keep pumping until all
    pending events have been processed. */
    DXUI_PUMP_ONCE_CLEAR,
    /* Pump until all windows have closed. */
    DXUI_PUMP_WHILE_WINDOWS_EXIST,
    /* Keep pumping forever (until an error occurs). */
    DXUI_PUMP_FOREVER,
};

/* Process pending events. The timeout is given in milliseconds. */
bool dxui_pump_events(dxui_context* /*context*/, int /*mode*/, int /*timeout*/);

void dxui_set_event_handler(dxui_control* /*control*/, int /*event*/,
        void* /*handler*/);
#define dxui_set_event_handler(control, event, handler) \
        dxui_set_event_handler(DXUI_AS_CONTROL(control), event, handler)

/* Containers. */

typedef union dxui_container {
    dxui_control* dxui_as_control;
    union dxui_container* dxui_as_container;
    struct dxui_internal_container* dxui_internal;
} dxui_container;

#define DXUI_AS_CONTAINER(container) ((container)->dxui_as_container)

void dxui_add_control(dxui_container* /*container*/, dxui_control* /*control*/);
#define dxui_add_control(container, control) \
        dxui_add_control(DXUI_AS_CONTAINER(container), DXUI_AS_CONTROL(control))

dxui_control* dxui_get_control_at(dxui_container* /*container*/,
        dxui_pos /*pos*/);
#define dxui_get_control_at(container, pos) \
        dxui_get_control_at(DXUI_AS_CONTAINER(container), pos)

/* Windows. */

typedef union {
    dxui_control* dxui_as_control;
    dxui_container* dxui_as_container;
    struct dxui_internal_window* dxui_internal;
} dxui_window;

void dxui_close(dxui_window* /*window*/);

enum {
    DXUI_WINDOW_NO_RESIZE = 1 << 0,
};

dxui_window* dxui_create_window(dxui_context* /*context*/, dxui_rect /*rect*/,
        const char* /*title*/, int /*flags*/);

/* Set the window to be drawn manually and return a framebuffer. */
dxui_color* dxui_get_framebuffer(dxui_window* /*window*/, dxui_dim /*dim*/);

void dxui_hide(dxui_window* /*window*/);

/* Switch back to automatic drawing and invalidate the framebuffer. */
void dxui_release_framebuffer(dxui_window* /*window*/);

void dxui_resize_window(dxui_window* /*window*/, dxui_dim /*dim*/);

enum {
    DXUI_CURSOR_ARROW,
    DXUI_CURSOR_RESIZE_DIAGONAL1,
    DXUI_CURSOR_RESIZE_DIAGONAL2,
    DXUI_CURSOR_RESIZE_HORIZONTAL,
    DXUI_CURSOR_RESIZE_VERTICAL,
};

void dxui_set_cursor(dxui_window* /*window*/, int /*cursor*/);

void dxui_show(dxui_window* /*window*/);

/* Update the given rect using data from the framebuffer. */
void dxui_update_framebuffer(dxui_window* /*window*/, dxui_rect /*rect*/);

/* Showing messages. */

void dxui_show_message(dxui_context* /*context*/, const char* /*text*/);

enum {
    DXUI_MSG_BOX_OK = 1 << 0,
    DXUI_MSG_BOX_YES = 1 << 1,
    DXUI_MSG_BOX_NO = 1 << 2,
    DXUI_MSG_BOX_CANCEL = 1 << 3,
};

int dxui_msg_box(dxui_context* /*context*/, const char* /*title*/,
        const char* /*text*/, int /*flags*/);

__attribute__((__noreturn__))
void dxui_panic(dxui_context* /*context*/, const char* /*text*/);

/* Specific controls. */

typedef union {
    dxui_control* dxui_as_control;
    struct dxui_internal_button* dxui_internal;
} dxui_button;

typedef union {
    dxui_control* dxui_as_control;
    struct dxui_internal_label* dxui_internal;
} dxui_label;

dxui_button* dxui_create_button(dxui_rect /*rect*/, const char* /*text*/);

dxui_label* dxui_create_label(dxui_rect /*rect*/, const char* /*text*/);

/* Rectangle functions. */

bool dxui_rect_contains_pos(dxui_rect /*rect*/, dxui_pos /*pos*/);

dxui_rect dxui_rect_crop(dxui_rect /*rect*/, dxui_dim /*dim*/);

bool dxui_rect_equals(dxui_rect /*a*/, dxui_rect /*b*/);

dxui_rect dxui_rect_intersect(dxui_rect /*a*/, dxui_rect /*b*/);

/* Text handling. */

enum {
    DXUI_TEXT_CENTERED = 1 << 0,
};

dxui_rect dxui_get_text_rect(const char* /*text*/, dxui_rect /*rect*/,
        int /*flags*/);

void dxui_draw_text(dxui_context* /*context*/, dxui_color* /*framebuffer*/,
        const char* /*text*/, dxui_color /*color*/, dxui_rect /*rect*/,
        dxui_rect /*crop*/, size_t /*pitch*/, int /*flags*/);

void dxui_draw_text_in_rect(dxui_context* /*context*/,
        dxui_color* /*framebuffer*/, const char* /*text*/, dxui_color /*color*/,
        dxui_pos /*pos*/, dxui_rect /*rect*/, size_t /*pitch*/);

#ifdef __cplusplus
}
#endif

#endif
