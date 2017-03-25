/* Copyright (c) 2016, 2017 Dennis Wölfing
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

/* kernel/src/keyboard.cpp
 * Keyboard.
 */

#include <stddef.h>
#include <dennix/kbkeys.h>
#include <dennix/kernel/keyboard.h>

// US keyboard layout.
static const char KBLAYOUT_US[] = {
    // No modifiers, Shift, Caps, Shift + Caps,
    0, 0, 0, 0,
    0, 0, 0, 0, // Escape
    '1', '!', '1', '!',
    '2', '@', '2', '@',
    '3', '#', '3', '#',
    '4', '$', '4', '$',
    '5', '%', '5', '%',
    '6', '^', '6', '^',
    '7', '&', '7', '&',
    '8', '*', '8', '*',
    '9', '(', '9', '(',
    '0', ')', '0', ')',
    '-', '_', '-', '_',
    '=', '+', '=', '+',
    '\b', '\b', '\b', '\b',
    '\t', '\t', '\t', '\t',
    'q', 'Q', 'Q', 'q',
    'w', 'W', 'W', 'w',
    'e', 'E', 'E', 'e',
    'r', 'R', 'R', 'r',
    't', 'T', 'T', 't',
    'y', 'Y', 'Y', 'y',
    'u', 'U', 'U', 'u',
    'i', 'I', 'I', 'i',
    'o', 'O', 'O', 'o',
    'p', 'P', 'P', 'p',
    '[', '{', '[', '{',
    ']', '}', ']', '}',
    '\n', '\n', '\n', '\n',
    0, 0, 0, 0, // left Control
    'a', 'A', 'A', 'a',
    's', 'S', 'S', 's',
    'd', 'D', 'D', 'd',
    'f', 'F', 'F', 'f',
    'g', 'G', 'G', 'g',
    'h', 'H', 'H', 'h',
    'j', 'J', 'J', 'j',
    'k', 'K', 'K', 'k',
    'l', 'L', 'L', 'l',
    ';', ':', ';', ':',
    '\'', '"', '\'', '"',
    '`', '~', '`', '~',
    0, 0, 0, 0, // left Shift
    '\\', '|', '\\', '|',
    'z', 'Z', 'Z', 'z',
    'x', 'X', 'X', 'x',
    'c', 'C', 'C', 'c',
    'v', 'V', 'V', 'v',
    'b', 'B', 'B', 'b',
    'n', 'N', 'N', 'n',
    'm', 'M', 'M', 'm',
    ',', '<', ',', '<',
    '.', '>', '.', '>',
    '/', '?', '/', '?',
    0, 0, 0, 0, // right Shift
    '*', '*', '*', '*',
    0, 0, 0, 0, // left Alt
    ' ', ' ', ' ', ' ',
    0, 0, 0, 0, // Caps Lock
    0, 0, 0, 0, // F1
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0, // F10
    0, 0, 0, 0, // Num Lock
    0, 0, 0, 0, // Scroll Lock
    '7', 0, '7', 0,
    '8', 0, '8', 0,
    '9', 0, '9', 0,
    '-', '-', '-', '-',
    '4', 0, '4', 0,
    '5', 0, '5', 0,
    '6', 0, '6', 0,
    '+', '+', '+', '+',
    '1', 0, '1', 0,
    '2', 0, '2', 0,
    '3', 0, '3', 0,
    '0', 0, '0', 0,
    '.', 0, '.', 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0, // F11
    0, 0, 0, 0, // F12
    // Most things below are not printable
};

static const struct {
    int key;
    const char* sequence;
} sequences[] = {
    { KB_UP, "\e[A" },
    { KB_DOWN, "\e[B" },
    { KB_RIGHT, "\e[C" },
    { KB_LEFT, "\e[D" },
    { KB_END, "\e[F" },
    { KB_HOME, "\e[H" },
    { KB_INSERT, "\e[2~" },
    { KB_DELETE, "\e[3~" },
    { KB_PAGEUP, "\e[5~" },
    { KB_PAGEDOWN, "\e[6~" },
    { KB_F1, "\e[OP" },
    { KB_F2, "\e[OQ" },
    { KB_F3, "\e[OR" },
    { KB_F4, "\e[OS" },
    { KB_F5, "\e[15~" },
    { KB_F6, "\e[17~" },
    { KB_F7, "\e[18~" },
    { KB_F8, "\e[19~" },
    { KB_F9, "\e[20~" },
    { KB_F10, "\e[21~" },
    { KB_F11, "\e[23~" },
    { KB_F12, "\e[24~" },
    { 0, 0 }
};

char Keyboard::getCharFromKey(int key) {
    static bool leftShift = false;
    static bool rightShift = false;
    static bool capsLock = false;
    static bool leftControl = false;
    static bool rightControl = false;

    if (key == KB_LSHIFT) {
        leftShift = true;
    } else if (key == KB_RSHIFT) {
        rightShift = true;
    } else if (key == KB_CAPSLOCK) {
        capsLock = !capsLock;
    } else if (key == KB_LCONTROL) {
        leftControl = true;
    } else if (key == KB_RCONTROL) {
        rightControl = true;
    } else if (key == -KB_LSHIFT) {
        leftShift = false;
    } else if (key == -KB_RSHIFT) {
        rightShift = false;
    } else if (key == -KB_LCONTROL) {
        leftControl = false;
    } else if (key == -KB_RCONTROL) {
        rightControl = false;
    }

    if (key < 0) return '\0';

    char result = '\0';
    size_t index = key << 2 | (capsLock << 1) | (leftShift || rightShift);
    if (index < sizeof(KBLAYOUT_US)) {
        result = KBLAYOUT_US[index];
    } else if (key == KB_NUMPAD_ENTER) {
        result = '\n';
    } else if (key == KB_NUMPAD_DIV) {
        result = '/';
    }

    if (leftControl || rightControl) {
        result = result & 0x1F;
    }

    return result;
}

const char* Keyboard::getSequenceFromKey(int key) {
    for (size_t i = 0; sequences[i].key != 0; i++) {
        if (sequences[i].key == key) {
            return sequences[i].sequence;
        }
    }

    return NULL;
}
