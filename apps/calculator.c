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

/* apps/calculator.c
 * A graphical calculator. Only integer arithmetic is supported.
 */

#include <dxui.h>
#include <inttypes.h>
#include <stdlib.h>

static dxui_context* context;
static dxui_label* label;
static int64_t value = 0;
static char operator = '\0';
static int64_t operand = 0;
static bool error = false;
static bool operandEntered = false;
static bool valueIsResult = false;

#define MAX_VALUE 999999999999999999LL
#define MIN_VALUE (-999999999999999999LL)

static void calculate(void);
static bool createWindow(void);
static void handleInput(char c);
static void onButtonClick(dxui_control* control, dxui_mouse_event* event);
static void onExitButtonClick(dxui_control* control, dxui_mouse_event* event);
static void onKey(dxui_control* control, dxui_key_event* event);
static void onPow2ButtonClick(dxui_control* control, dxui_mouse_event* event);
static bool outOfRange(int64_t x);
static void shutdown(void);
static void updateDisplay(void);

int main(void) {
    atexit(shutdown);
    context = dxui_initialize(DXUI_INIT_CURSOR);
    if (!context) dxui_panic(NULL, "Failed to initialize dxui.");

    if (!createWindow()) {
        dxui_panic(context, "Failed to create calculator window.");
    }

    updateDisplay();
    dxui_pump_events(context, DXUI_PUMP_WHILE_WINDOWS_EXIST, -1);
}

static void calculate(void) {
    if (!operandEntered) {
        operand = value;
    }
    if (error) {
        value = 0;
        error = false;
        operator = '\0';
    }
    if (operator == '+') {
        error = __builtin_add_overflow(value, operand, &value);
    } else if (operator == '-') {
        error = __builtin_sub_overflow(value, operand, &value);
    } else if (operator == '*') {
        error = __builtin_mul_overflow(value, operand, &value);
    } else if (operator == '/') {
        if (operand == 0) {
            error = true;
        } else {
            value /= operand;
        }
    }

    if (outOfRange(value)) {
        error = true;
    }

    operator = '\0';
    operand = 0;
    operandEntered = false;
    valueIsResult = true;
}

static bool createWindow(void) {
    dxui_rect rect = {{ -1, -1, 200, 195 }};
    dxui_window* window = dxui_create_window(context, rect, "Calculator",
            DXUI_WINDOW_NO_RESIZE);
    if (!window) return false;

    rect = (dxui_rect) {{ 10, 10, 180, 25 }};
    label = dxui_create_label(rect, "");
    if (!label) return false;
    dxui_set_background(label, COLOR_WHITE);
    dxui_add_control(window, label);

    rect = (dxui_rect) {{ 11, 40, 40, 25 }};
    dxui_button* button = dxui_create_button(rect, "x²");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onPow2ButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 57, 40, 40, 25 }};
    button = dxui_create_button(rect, "/");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 103, 40, 40, 25 }};
    button = dxui_create_button(rect, "*");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 149, 40, 40, 25 }};
    button = dxui_create_button(rect, "-");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 11, 70, 40, 25 }};
    button = dxui_create_button(rect, "7");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 57, 70, 40, 25 }};
    button = dxui_create_button(rect, "8");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 103, 70, 40, 25 }};
    button = dxui_create_button(rect, "9");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 149, 70, 40, 55 }};
    button = dxui_create_button(rect, "+");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 11, 100, 40, 25 }};
    button = dxui_create_button(rect, "4");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 57, 100, 40, 25 }};
    button = dxui_create_button(rect, "5");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 103, 100, 40, 25 }};
    button = dxui_create_button(rect, "6");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 11, 130, 40, 25 }};
    button = dxui_create_button(rect, "1");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 57, 130, 40, 25 }};
    button = dxui_create_button(rect, "2");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 103, 130, 40, 25 }};
    button = dxui_create_button(rect, "3");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 149, 130, 40, 55 }};
    button = dxui_create_button(rect, "=");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    rect = (dxui_rect) {{ 57, 160, 40, 25 }};
    button = dxui_create_button(rect, "0");
    if (!button) return false;
    dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK, onButtonClick);
    dxui_add_control(window, button);

    if (dxui_is_standalone(context)) {
        // The exit button is only needed when running without the compositor.
        rect = (dxui_rect) {{ 11, 160, 40, 25 }};
        button = dxui_create_button(rect, "Exit");
        if (!button) return false;
        dxui_set_event_handler(button, DXUI_EVENT_MOUSE_CLICK,
                onExitButtonClick);
        dxui_add_control(window, button);
    }

    dxui_set_event_handler(window, DXUI_EVENT_KEY, onKey);
    dxui_show(window);
    return true;
}

static void handleInput(char c) {
    if (c >= '0' && c <= '9') {
        if (error || valueIsResult) {
            value = c - '0';
            error = false;
            valueIsResult = false;
        } else if (operator) {
            if (operand > MAX_VALUE / 10) {
                error = true;
            } else {
                operand = operand * 10 + (c - '0');
                operandEntered = true;
            }
        } else {
            if (value > MAX_VALUE / 10) {
                error = true;
            } else {
                value = value * 10 + (c - '0');
            }
        }
    } else if (c == '=' || c == '\n') {
        calculate();
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
        if (operandEntered) {
            calculate();
        }
        valueIsResult = false;
        operator = c;
    }

    updateDisplay();
}

static void onButtonClick(dxui_control* control, dxui_mouse_event* event) {
    (void) event;
    handleInput(dxui_get_text(control)[0]);
}

static void onExitButtonClick(dxui_control* control, dxui_mouse_event* event) {
    (void) control; (void) event;
    exit(0);
}

static void onKey(dxui_control* control, dxui_key_event* event) {
    (void) control;
    handleInput(event->codepoint);
}

static void onPow2ButtonClick(dxui_control* control, dxui_mouse_event* event) {
    (void) control; (void) event;
    if (operator) {
        if (!operandEntered) {
            operand = value;
            operandEntered = true;
        }
        if (__builtin_mul_overflow(operand, operand, &operand) ||
                outOfRange(operand)) {
            error = true;
        }
    } else {
        if (__builtin_mul_overflow(value, value, &value) || outOfRange(value)) {
            error = true;
        }
        valueIsResult = true;
    }
    updateDisplay();
}

static bool outOfRange(int64_t x) {
    return x > MAX_VALUE || x < MIN_VALUE;
}

static void shutdown(void) {
    dxui_shutdown(context);
}

static void updateDisplay(void) {
    if (error) {
        dxui_set_text(label, "Error");
    } else {
        dxui_set_text_format(label, "%20"PRId64,
                operandEntered ? operand : value);
    }
}
