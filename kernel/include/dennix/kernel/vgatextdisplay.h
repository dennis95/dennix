/* Copyright (c) 2018 Dennis Wölfing
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

/* kernel/include/dennix/kernel/vgatextdisplay.h
 * VGA text mode display.
 */

#ifndef KERNEL_VGATEXTDISPLAY_H
#define KERNEL_VGATEXTDISPLAY_H

#include <dennix/kernel/textdisplay.h>

class VgaTextDisplay : public TextDisplay {
public:
    VgaTextDisplay();
    virtual void clear(CharPos from, CharPos to, uint8_t color);
    virtual void putCharacter(CharPos position, wchar_t c, uint8_t color);
    virtual void scroll(unsigned int lines, uint8_t color, bool up = true);
    virtual void setCursorPos(CharPos position);
};

#endif
