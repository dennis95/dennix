/* Copyright (c) 2017, 2018, 2020 Dennis Wölfing
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

/* kernel/include/dennix/kernel/terminaldisplay.h
 * Terminal display with support for ECMA-48 terminal escapes.
 */

#ifndef KERNEL_TERMINALDISPLAY_H
#define KERNEL_TERMINALDISPLAY_H

#include <dennix/kernel/display.h>

namespace TerminalDisplay {
extern Reference<Display> display;

void backspace();
void printCharacter(char c);
void printCharacterRaw(char c);
void updateCursorPosition();
void updateDisplaySize();
}

#endif
