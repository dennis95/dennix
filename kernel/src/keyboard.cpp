/* Copyright (c) 2016, Dennis Wölfing
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
#include <dennix/kernel/keyboard.h>

// German keyboard layout.
static const char KBLAYOUT_DE[] = {
    0, 0, 0, 0,
    0, 0, 0, 0, // Escape
    '1', '!', 0, 0,
    '2', '"', 0/*'²'*/, 0,
    '3', 0/*'§'*/, 0/*'³'*/, 0,
    '4', '$', 0, 0,
    '5', '%', 0, 0,
    '6', '&', 0, 0,
    '7', '/', '{', 0,
    '8', '(', '[', 0,
    '9', ')', ']', 0,
    '0', '=', '}', 0,
    0/*'ß'*/, '?', '\\', 0,
    0/*'´'*/, '`', 0, 0,
    '\b', '\b', '\b', '\b',
    '\t', '\t', '\t', '\t',
    'q', 'Q', '@', 0,
    'w', 'W', 0, 0,
    'e', 'E', 0/*'€'*/, 0,
    'r', 'R', 0, 0,
    't', 'T', 0, 0,
    'z', 'Z', 0, 0,
    'u', 'U', 0, 0,
    'i', 'I', 0, 0,
    'o', 'O', 0, 0,
    'p', 'P', 0, 0,
    0/*'ü'*/, 0/*'Ü'*/, 0, 0,
    '+', '*', '~', 0,
    '\n', '\n', '\n', '\n',
    0, 0, 0, 0, // left Control
    'a', 'A', 0, 0,
    's', 'S', 0, 0,
    'd', 'D', 0, 0,
    'f', 'F', 0, 0,
    'g', 'G', 0, 0,
    'h', 'H', 0, 0,
    'j', 'J', 0, 0,
    'k', 'K', 0, 0,
    'l', 'L', 0, 0,
    0/*'ö'*/, 0/*'Ö'*/, 0, 0,
    0/*'ä'*/, 0/*'Ä'*/, 0, 0,
    '^', 0/*'°'*/, 0, 0,
    0, 0, 0, 0, // left Shift
    '#', '\'', 0, 0,
    'y', 'Y', 0, 0,
    'x', 'X', 0, 0,
    'c', 'C', 0, 0,
    'v', 'V', 0, 0,
    'b', 'B', 0, 0,
    'n', 'N', 0, 0,
    'm', 'M', 0/*'µ'*/, 0,
    ',', ';', 0, 0,
    '.', ':', 0, 0,
    '-', '_', 0, 0,
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
    ',', 0, ',', 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    '<', '>', '|', 0,
    0, 0, 0, 0, // F11
    0, 0, 0, 0, // F12
    // Most things below are not printable
};

char Keyboard::getCharFromKey(int key) {
    static bool leftShift = false;
    static bool rightShift = false;
    static bool capsLock = false;
    static bool altGr = false;

    if (key == 0x2A) {
        leftShift = true;
    } else if (key == 0x36) {
        rightShift = true;
    } else if (key == 0x3A) {
        capsLock = !capsLock;
    } else if (key == 0xB8) {
        altGr = true;
    } else if (key == -0x2A) {
        leftShift = false;
    } else if (key == -0x36) {
        rightShift = false;
    } else if (key == -0xB8) {
        altGr = false;
    }

    if (key < 0) return '\0';

    size_t index = key << 2 | altGr << 1 | ((leftShift || rightShift) ^ capsLock);
    if (index < sizeof(KBLAYOUT_DE)) {
        return KBLAYOUT_DE[index];
    }

    if (key == 0x9C) {
        return '\n';
    } else if (key == 0xB5) {
        return '/';
    }

    return '\0';
}
