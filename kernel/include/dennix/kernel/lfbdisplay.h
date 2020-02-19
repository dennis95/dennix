/* Copyright (c) 2018, 2019, 2020 Dennis Wölfing
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

/* kernel/include/dennix/kernel/lfbdisplay.h
 * Linear frame buffer display.
 */

#ifndef KERNEL_LFBDISPLAY_H
#define KERNEL_LFBDISPLAY_H

#include <dennix/kernel/display.h>

struct CharBufferEntry {
    wchar_t wc;
    uint8_t color;
    bool modified;

    bool operator!=(const CharBufferEntry& other) {
        return wc != other.wc || color != other.color;
    }
};

class LfbDisplay : public Display {
public:
    LfbDisplay(char* lfb, size_t pixelWidth, size_t pixelHeight,
            size_t pitch, size_t bpp);
    virtual void clear(CharPos from, CharPos to, uint8_t color);
    virtual int devctl(int command, void* restrict data, size_t size,
            int* restrict info);
    virtual void putCharacter(CharPos position, wchar_t c, uint8_t color);
    virtual void scroll(unsigned int lines, uint8_t color, bool up = true);
    virtual void setCursorPos(CharPos position);
    virtual void update();
private:
    char* charAddress(CharPos position);
    void redraw(CharPos position);
    void setPixelColor(char* addr, uint32_t rgbColor);
private:
    char* lfb;
    size_t pixelWidth;
    size_t pixelHeight;
    size_t pitch;
    size_t bpp;
    CharPos cursorPos;
    CharBufferEntry* doubleBuffer;
    bool invalidated;
    bool renderingText;
};

#endif
