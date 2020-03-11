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

/* kernel/src/lfbdisplay.cpp
 * Linear frame buffer display.
 */

#include <errno.h>
#include <dennix/display.h>
#include <dennix/kernel/lfbdisplay.h>

// Classical VGA font but with the Unicode replacement character at 0xFF.
asm(".pushsection .rodata\n"
".global vgafont\n"
"vgafont:\n\t"
    ".incbin \"../vgafont\"\n"
".popsection");
extern const uint8_t vgafont[];

static const size_t charHeight = 16;
static const size_t charWidth = 9;

LfbDisplay::LfbDisplay(char* lfb, size_t pixelWidth, size_t pixelHeight,
        size_t pitch, size_t bpp) {
    this->lfb = lfb;
    _height = pixelHeight / charHeight;
    _width = (pixelWidth + 1) / charWidth;
    this->pixelHeight = pixelHeight;
    this->pixelWidth = pixelWidth;
    this->pitch = pitch;
    this->bpp = bpp;
    cursorPos = {0, 0};
    doubleBuffer = nullptr;
    invalidated = false;
    renderingText = true;
}

ALWAYS_INLINE char* LfbDisplay::charAddress(CharPos position) {
    return lfb + position.y * charHeight * pitch +
            position.x * charWidth * bpp / 8;
}

ALWAYS_INLINE void LfbDisplay::setPixelColor(char* addr,
        uint32_t rgbColor) {
    if ((rgbColor & 0xFF000000) == 0) return;
    if (bpp == 32) {
        uint32_t* pixel = (uint32_t*) addr;
        *pixel = rgbColor;
    } else {
        addr[0] = (rgbColor >> 0) & 0xFF;
        addr[1] = (rgbColor >> 8) & 0xFF;
        addr[2] = (rgbColor >> 16) & 0xFF;
    }
}

void LfbDisplay::clear(CharPos from, CharPos to, Color color) {
    size_t bufferStart = from.x + width() * from.y;
    size_t bufferEnd = to.x + width() * to.y;
    for (size_t i = bufferStart; i <= bufferEnd; i++) {
        if (doubleBuffer[i].wc != L'\0' ||
                doubleBuffer[i].fgColor != color.fgColor ||
                doubleBuffer[i].bgColor != color.bgColor) {
            doubleBuffer[i].wc = L'\0';
            doubleBuffer[i].fgColor = color.fgColor;
            doubleBuffer[i].bgColor = color.bgColor;
            doubleBuffer[i].modified = true;
        }
    }
}

void LfbDisplay::initialize() {
    doubleBuffer = xnew CharBufferEntry[_height * _width];
    Color defaultColor = { RGB(170, 170, 170), RGB(0, 0, 0), 0x07 };
    clear({0, 0}, {_width - 1, _height - 1}, defaultColor);
}

void LfbDisplay::putCharacter(CharPos position, wchar_t wc, Color color) {
    if (unlikely(!doubleBuffer)) {
        CharBufferEntry entry = { wc, color.fgColor, color.bgColor, true };
        redraw(position, &entry);
        return;
    }

    doubleBuffer[position.x + width() * position.y].wc = wc;
    doubleBuffer[position.x + width() * position.y].fgColor = color.fgColor;
    doubleBuffer[position.x + width() * position.y].bgColor = color.bgColor;
    doubleBuffer[position.x + width() * position.y].modified = true;
}

void LfbDisplay::redraw(CharPos position) {
    CharBufferEntry* entry = &doubleBuffer[position.x + width() * position.y];
    redraw(position, entry);
}

void LfbDisplay::redraw(CharPos position, CharBufferEntry* entry) {
    entry->modified = false;
    wchar_t wc = entry->wc;

    uint32_t foreground = entry->fgColor;
    uint32_t background = entry->bgColor;
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

void LfbDisplay::scroll(unsigned int lines, Color color, bool up /*= true*/) {
    CharBufferEntry empty;
    empty.wc = L'\0';
    empty.fgColor = color.fgColor;
    empty.bgColor = color.bgColor;

    if (up) {
        for (unsigned int y = 0; y < height(); y++) {
            for (unsigned int x = 0; x < width(); x++) {
                CharBufferEntry& entry = y < height() - lines ?
                        doubleBuffer[x + (y + lines) * width()] : empty;
                if (doubleBuffer[x + y * width()] != entry) {
                    doubleBuffer[x + y * width()].wc = entry.wc;
                    doubleBuffer[x + y * width()].fgColor = entry.fgColor;
                    doubleBuffer[x + y * width()].bgColor = entry.bgColor;
                    doubleBuffer[x + y * width()].modified = true;
                }
            }
        }
    } else {
        for (unsigned int y = height() - 1; y < height(); y--) {
            for (unsigned int x = 0; x < width(); x++) {
                CharBufferEntry& entry = y >= lines ?
                        doubleBuffer[x + (y - lines) * width()] : empty;
                if (doubleBuffer[x + y * width()] != entry) {
                    doubleBuffer[x + y * width()].wc = entry.wc;
                    doubleBuffer[x + y * width()].fgColor = entry.fgColor;
                    doubleBuffer[x + y * width()].bgColor = entry.bgColor;
                    doubleBuffer[x + y * width()].modified = true;
                }
            }
        }
    }
}

void LfbDisplay::setCursorPos(CharPos position) {
    if (unlikely(!doubleBuffer)) return;
    CharPos oldPos = cursorPos;
    cursorPos = position;
    doubleBuffer[oldPos.x + oldPos.y * width()].modified = true;
    doubleBuffer[cursorPos.x + cursorPos.y * width()].modified = true;
}

void LfbDisplay::update() {
    if (!renderingText || !doubleBuffer) return;
    bool redrawAll = invalidated;
    invalidated = false;

    for (unsigned int y = 0; y < height(); y++) {
        for (unsigned int x = 0; x < width(); x++) {
            if (redrawAll || doubleBuffer[x + y * width()].modified) {
                redraw({x, y});
            }
        }
    }
}

int LfbDisplay::devctl(int command, void* restrict data, size_t size,
        int* restrict info) {
    switch (command) {
    case DISPLAY_SET_MODE: {
        if (size != 0 && size != sizeof(int)) {
            *info = -1;
            return EINVAL;
        }

        const int* mode = (const int*) data;

        if (*mode == DISPLAY_MODE_QUERY) {
            *info = renderingText ? DISPLAY_MODE_TEXT : DISPLAY_MODE_LFB;
            return 0;
        } else if (*mode == DISPLAY_MODE_TEXT) {
            if (!renderingText) {
                invalidated = true;
            }
            renderingText = true;
            *info = DISPLAY_MODE_TEXT;
            return 0;
        } else if (*mode == DISPLAY_MODE_LFB) {
            renderingText = false;
            *info = DISPLAY_MODE_LFB;
            return 0;
        } else {
            *info = renderingText ? DISPLAY_MODE_TEXT : DISPLAY_MODE_LFB;
            return ENOTSUP;
        }
    } break;
    case DISPLAY_GET_RESOLUTION: {
        if (size != 0 && size != sizeof(struct display_resolution)) {
            *info = -1;
            return EINVAL;
        }

        struct display_resolution* res = (struct display_resolution*) data;
        res->width = pixelWidth;
        res->height = pixelHeight;
        *info = 0;
        return 0;
    } break;
    case DISPLAY_DRAW: {
        if (size != 0 && size != sizeof(struct display_draw)) {
            *info = -1;
            return EINVAL;
        }

        if (renderingText) {
            *info = -1;
            return ENOTSUP;
        }

        const struct display_draw* draw = (const struct display_draw*) data;
        for (size_t y = 0; y < draw->draw_height; y++) {
            const uint32_t* row = (const uint32_t*) ((uintptr_t) draw->lfb +
                    (draw->draw_y + y) * draw->lfb_pitch);
            for (size_t x = 0; x < draw->draw_width; x++) {
                char* addr = lfb + (y + draw->lfb_y + draw->draw_y) * pitch +
                        (x + draw->lfb_x + draw->draw_x) * bpp / 8;
                setPixelColor(addr, row[draw->draw_x + x]);
            }
        }

        *info = 0;
        return 0;
    } break;
    default:
        *info = -1;
        return EINVAL;
    }
}
