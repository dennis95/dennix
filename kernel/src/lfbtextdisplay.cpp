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

/* kernel/src/lfbtextdisplay.cpp
 * Linear frame buffer text display.
 */


#include <string.h>
#include <dennix/kernel/lfbtextdisplay.h>

// Classical VGA font but with the Unicode replacement character at 0xFF.
asm(".pushsection .rodata\n"
"vgafont:\n\t"
    ".incbin \"../vgafont\"\n"
".popsection");
extern const uint8_t vgafont[];

static const size_t charHeight = 16;
static const size_t charWidth = 9;

#define RGB(r, g, b) (((r) << 16) | ((g) << 8) | (b))

static uint32_t vgaColors[16] = {
    RGB(0, 0, 0),
    RGB(0, 0, 170),
    RGB(0, 170, 0),
    RGB(0, 170, 170),
    RGB(170, 0, 0),
    RGB(170, 0, 170),
    RGB(170, 85, 0),
    RGB(170, 170, 170),
    RGB(85, 85, 85),
    RGB(85, 85, 255),
    RGB(85, 255, 85),
    RGB(85, 255, 255),
    RGB(255, 85, 85),
    RGB(255, 85, 255),
    RGB(255, 255, 85),
    RGB(255, 255, 255),
};

LfbTextDisplay::LfbTextDisplay(char* lfb, size_t pixelWidth, size_t pixelHeight,
        size_t pitch, size_t bpp) {
    this->lfb = lfb;
    _height = pixelHeight / charHeight;
    _width = (pixelWidth + 1) / charWidth;
    this->pixelHeight = pixelHeight;
    this->pixelWidth = pixelWidth;
    this->pitch = pitch;
    this->bpp = bpp;
    cursorPos = {0, 0};
    doubleBuffer = new CharBufferEntry[_height * _width];
    clear({0, 0}, {_width - 1, _height - 1}, 0x07);
}

ALWAYS_INLINE char* LfbTextDisplay::charAddress(CharPos position) {
    return lfb + position.y * charHeight * pitch +
            position.x * charWidth * bpp / 8;
}

ALWAYS_INLINE void LfbTextDisplay::setPixelColor(char* addr,
        uint32_t rgbColor) {
    if (bpp == 32) {
        uint32_t* pixel = (uint32_t*) addr;
        *pixel = rgbColor;
    } else {
        addr[0] = (rgbColor >> 0) & 0xFF;
        addr[1] = (rgbColor >> 8) & 0xFF;
        addr[2] = (rgbColor >> 16) & 0xFF;
    }
}

void LfbTextDisplay::clear(CharPos from, CharPos to, uint8_t color) {
    size_t bufferStart = from.x + width() * from.y;
    size_t bufferEnd = to.x + width() * to.y;
    for (size_t i = bufferStart; i <= bufferEnd; i++) {
        doubleBuffer[i].wc = L'\0';
        doubleBuffer[i].color = color;
    }

    uint32_t rgbColor = vgaColors[color & 0xF0];
    char* addr = lfb + 16 * from.y * pitch;

    for (size_t i = from.y * 16; i < 16 * (to.y + 1); i++) {
        size_t start = i < (from.y + 1) * 16 ? from.x * charWidth : 0;
        size_t end = i >= to.y * 16 ? (to.x + 1) * charWidth : pixelWidth;
        if (end == pixelWidth + 1) {
            end--;
        }
        for (size_t j = start; j < end; j++) {
            setPixelColor(&addr[j * bpp / 8], rgbColor);
        }
        addr += pitch;
    }

    redraw(cursorPos);
}

void LfbTextDisplay::putCharacter(CharPos position, wchar_t wc, uint8_t color) {
    doubleBuffer[position.x + width() * position.y].wc = wc;
    doubleBuffer[position.x + width() * position.y].color = color;

    redraw(position);
}

void LfbTextDisplay::redraw(CharPos position) {
    wchar_t wc = doubleBuffer[position.x + width() * position.y].wc;
    uint8_t color = doubleBuffer[position.x + width() * position.y].color;

    uint32_t foreground = vgaColors[color & 0x0F];
    uint32_t background = vgaColors[(color >> 4) & 0x0F];
    uint8_t cp437 = unicodeToCp437(wc);
    const uint8_t* charFont = vgafont + cp437 * 16;
    char* addr = charAddress(position);

    for (size_t i = 0; i < 16; i++) {
        for (size_t j = 0; j < 8; j++) {
            bool pixelFg = charFont[i] & (1 << (7 - j)) ||
                    (position == cursorPos && i >= 14);
            uint32_t rgbColor = pixelFg ? foreground : background;
            setPixelColor(&addr[j * bpp / 8], rgbColor);
        }

        if (likely((position.x + 1) * charWidth <= pixelWidth)) {
            bool pixelFg = cp437 >= 0xB0 && cp437 <= 0xDF && charFont[i] & 1;
            uint32_t rgbColor = pixelFg ? foreground : background;
            setPixelColor(&addr[8 * bpp / 8], rgbColor);
        }
        addr += pitch;
    }
}

void LfbTextDisplay::redrawAll() {
    for (size_t y = 0; y < height(); y++) {
        for (size_t x = 0; x < width(); x++) {
            redraw({x, y});
        }
    }
}

void LfbTextDisplay::scroll(unsigned int lines, uint8_t color,
        bool up /*= true*/) {
    size_t size = (height() - lines) * width() * sizeof(CharBufferEntry);

    if (up) {
        memmove(doubleBuffer, doubleBuffer + lines * width(), size);
        for (size_t y = height() - lines; y < height(); y++) {
            for (size_t x = 0; x < width(); x++) {
                doubleBuffer[x + y * width()].wc = L'\0';
                doubleBuffer[x + y * width()].color = color;
            }
        }
    } else {
        memmove(doubleBuffer + lines * width(), doubleBuffer, size);
        for (size_t y = 0; y < lines; y++) {
            for (size_t x = 0; x < width(); x++) {
                doubleBuffer[x + y * width()].wc = L'\0';
                doubleBuffer[x + y * width()].color = color;
            }
        }
    }

    redrawAll();
}

void LfbTextDisplay::setCursorPos(CharPos position) {
    CharPos oldPos = cursorPos;
    cursorPos = position;
    redraw(oldPos);
    redraw(cursorPos);
}
